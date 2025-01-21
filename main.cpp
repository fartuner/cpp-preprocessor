#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using filesystem::path;

path operator""_p(const char* data, std::size_t sz) {
    return path(data, data + sz);
}

bool ProcessIncludes(const path& file, const path& original_file, int original_line,
                    const vector<path>& include_directories, ofstream& out_stream) {
    ifstream in_stream(file);
    if (!in_stream.is_open()) {
        if (original_line > 0) {
            cout << "unknown include file " << file.filename().string() << " at file "
                 << original_file.string() << " at line " << original_line << endl;
        }
        return false;
    }

    string line;
    int current_line = 0;
    static const regex include_regex(R"(\s*#\s*include\s*\"([^\"]+)\"\s*)");
    static const regex angle_include_regex(R"(\s*#\s*include\s*<([^>]+)>\s*)");

    while (getline(in_stream, line)) {
        current_line++;
        smatch match;
        bool processed = false;

        if (regex_match(line, match, include_regex)) {
            path included_file = match[1].str();
            path resolved_path = file.parent_path() / included_file;
            
            ifstream test_file(resolved_path);
            if (test_file.is_open()) {
                test_file.close();
                if (!ProcessIncludes(resolved_path, file, current_line, include_directories, out_stream)) {
                    return false;
                }
                processed = true;
            } else {
                bool found = false;
                for (const auto& dir : include_directories) {
                    resolved_path = dir / included_file;
                    test_file.open(resolved_path);
                    if (test_file.is_open()) {
                        test_file.close();
                        if (!ProcessIncludes(resolved_path, file, current_line, include_directories, out_stream)) {
                            return false;
                        }
                        found = true;
                        processed = true;
                        break;
                    }
                }
                if (!found) {
                    cout << "unknown include file " << included_file.filename().string() << " at file "
                         << file.string() << " at line " << current_line << endl;
                    return false;
                }
            }
        } else if (regex_match(line, match, angle_include_regex)) {
            path included_file = match[1].str();
            bool found = false;

            for (const auto& dir : include_directories) {
                path resolved_path = dir / included_file;
                ifstream test_file(resolved_path);
                if (test_file.is_open()) {
                    test_file.close();
                    if (!ProcessIncludes(resolved_path, file, current_line, include_directories, out_stream)) {
                        return false;
                    }
                    found = true;
                    processed = true;
                    break;
                }
            }

            if (!found) {
                cout << "unknown include file " << included_file.filename().string() << " at file "
                     << file.string() << " at line " << current_line << endl;
                return false;
            }
        }

        if (!processed) {
            out_stream << line << endl;
        }
    }

    return true;
}

bool Preprocess(const path& in_file, const path& out_file,
                const vector<path>& include_directories) {
    ifstream in_stream(in_file);
    if (!in_stream.is_open()) {
        return false;
    }

    ofstream out_stream(out_file);
    if (!out_stream.is_open()) {
        return false;
    }

    return ProcessIncludes(in_file, in_file, 0, include_directories, out_stream);
}

string GetFileContents(const string& file) {
    ifstream stream(file);
    stringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

void Test() {
    error_code err;

    filesystem::remove_all("sources"_p, err);
    filesystem::create_directories("sources"_p / "include2"_p / "lib"_p, err);
    filesystem::create_directories("sources"_p / "include1"_p, err);
    filesystem::create_directories("sources"_p / "dir1"_p / "subdir"_p, err);

    {
        ofstream file("sources/a.cpp");
        file << "// this comment before include\n"
             "#include \"dir1/b.h\"\n"
             "// text between b.h and c.h\n"
             "#include \"dir1/d.h\"\n"
             "\n"
             "void SayHello() {\n"
             "    std::cout << \"hello, world!\" << std::endl;\n"
             "#   include<dummy.txt>\n"
             "}\n"s;
    }
    
    {
        ofstream file("sources/dir1/b.h");
        file << "// text from b.h before include\n"
             "#include \"subdir/c.h\"\n"
             "// text from b.h after include"s;
    }
    
    {
        ofstream file("sources/dir1/subdir/c.h");
        file << "// text from c.h before include\n"
             "#include <std1.h>\n"
             "// text from c.h after include\n"s;
    }
    
    {
        ofstream file("sources/dir1/d.h");
        file << "// text from d.h before include\n"
             "#include \"lib/std2.h\"\n"
             "// text from d.h after include\n"s;
    }
    
    {
        ofstream file("sources/include1/std1.h");
        file << "// std1\n"s;
    }
    
    {
        ofstream file("sources/include2/lib/std2.h");
        file << "// std2\n"s;
    }

    assert((!Preprocess("sources"_p / "a.cpp"_p,
                        "sources"_p / "a.in"_p,
                        {"sources"_p / "include1"_p,"sources"_p / "include2"_p})));

    ostringstream test_out;
    test_out << "// this comment before include\n"
              "// text from b.h before include\n"
              "// text from c.h before include\n"
              "// std1\n"
              "// text from c.h after include\n"
              "// text from b.h after include\n"
              "// text between b.h and c.h\n"
              "// text from d.h before include\n"
              "// std2\n"
              "// text from d.h after include\n"
              "\n"
              "void SayHello() {\n"
              "    std::cout << \"hello, world!\" << std::endl;\n"s;

    assert(GetFileContents("sources/a.in"s) == test_out.str());
}

int main() {
    Test();
}
