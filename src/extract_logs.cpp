#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <ctime>
#include <cstdio>

#ifdef _WIN32
#include <direct.h>
#include <sys/types.h>
#include <sys/stat.h>
#define fseeko _fseeki64
#define ftello _ftelli64
#define stat _stati64
#define mkdir(dir) _mkdir(dir)
#else
#include <sys/stat.h>
#include <unistd.h>
#define mkdir(dir) mkdir(dir, 0777)
#endif

using namespace std;

bool validate_date(const string& date_str) {
    if (date_str.length() != 10) return false;
    for (int i = 0; i < 10; i++) {
        if (i == 4 || i == 7) {
            if (date_str[i] != '-') return false;
        } else if (!isdigit(date_str[i])) {
            return false;
        }
    }
    return true;
}

string get_date_from_line(const char* line) {
    if (strlen(line) < 10) return "";
    return string(line).substr(0, 10);
}

uint64_t find_line_position(const char* filename, const string& target_date, bool find_start) {
#ifdef _WIN32
    struct _stati64 st;
    if (_stati64(filename, &st) != 0) {
#else
    struct stat st;
    if (stat(filename, &st) != 0) {
#endif
        cerr << "File not found: " << filename << endl;
        exit(1);
    }
    uint64_t file_size = st.st_size;

    FILE* file = fopen(filename, "rb");
    if (!file) {
        cerr << "Cannot open file: " << filename << endl;
        exit(1);
    }

    uint64_t low = 0, high = file_size - 1;

    while (low <= high) {
        uint64_t mid = low + (high - low)/2;
        fseeko(file, mid, SEEK_SET);

        // Fix: Align to line start
        if (mid > 0) {
            int c;
            // Read backward to find previous newline
            do {
                c = fgetc(file);
                if (c == '\n') break;
                fseeko(file, -2, SEEK_CUR);
            } while (ftello(file) > 0);
            
            if (c != '\n') {
                fseeko(file, 0, SEEK_SET);
            } else {
                fseeko(file, ftello(file) + 1, SEEK_SET);
            }
        }

        uint64_t line_start = ftello(file);
        char line[256];
        if (!fgets(line, sizeof(line), file)) break;

        string current_date = get_date_from_line(line);
        if (current_date.empty()) continue;

        // Update search bounds
        if (find_start) {
            if (current_date < target_date) {
                low = ftello(file);  // Move to next line
            } else {
                high = line_start - 1;
            }
        } else {
            if (current_date <= target_date) {
                low = ftello(file);  // Move to next line
            } else {
                high = line_start - 1;
            }
        }
    }

    // Linear scan for exact position
    fseeko(file, low, SEEK_SET);
    while (true) {
        uint64_t pos = ftello(file);
        char line[256];
        if (!fgets(line, sizeof(line), file)) break;

        string current_date = get_date_from_line(line);
        if (current_date.empty()) continue;

        if (find_start) {
            if (current_date == target_date) {
                fclose(file);
                return pos;
            } else if (current_date > target_date) break;
        } else {
            if (current_date > target_date) {
                fclose(file);
                return pos;
            }
        }
    }

    fclose(file);
    return find_start ? UINT64_MAX : file_size;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " YYYY-MM-DD\n";
        return 1;
    }

    string target_date = argv[1];
    if (!validate_date(target_date)) {
        cerr << "Invalid date format. Use YYYY-MM-DD.\n";
        return 1;
    }

    const char* input_file = "test_logs.log";
    string output_dir = "output";
    string output_file = output_dir + "/output_" + target_date + ".txt";

    if (mkdir(output_dir.c_str()) == -1 && errno != EEXIST) {
        cerr << "Error creating directory: " << strerror(errno) << endl;
        return 1;
    }

    uint64_t start_pos = find_line_position(input_file, target_date, true);
    if (start_pos == UINT64_MAX) {
        ofstream(output_file).close();
        cout << "No logs found for " << target_date << endl;
        return 0;
    }

    uint64_t end_pos = find_line_position(input_file, target_date, false);

    FILE* in_file = fopen(input_file, "rb");
    ofstream out_file(output_file, ios::binary);

    fseeko(in_file, start_pos, SEEK_SET);
    char buffer[4096];
    uint64_t remaining = end_pos - start_pos;

    while (remaining > 0) {
        size_t read_size = fread(buffer, 1, min(remaining, (uint64_t)4096), in_file);
        if (read_size <= 0) break;
        out_file.write(buffer, read_size);
        remaining -= read_size;
    }

    fclose(in_file);
    out_file.close();

    cout << "Logs saved to " << output_file << endl;
    return 0;
}