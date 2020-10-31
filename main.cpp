#include <iostream>
#include <iomanip>
#include <string>
#include <map>
#include <vector>
#include <algorithm>
#include "pwd.h"
#include "ftw.h"
#include "sys/stat.h"
#include <unistd.h>


using file_t = std::pair<std::string, struct stat>;
using files_t = std::vector<file_t>;
static std::map<std::string, files_t> storage;

static int saveInStorage(const char *fpath, const struct stat *sb, int tflag, struct FTW *ftwbuf);

bool comparator(const std::string& str1, const std::string& str2);

std::string getUserName(uid_t uid);

bool isExec(struct stat st);

void printFile(const file_t& file);

int main(int argc, char *argv[])
{
    if (argc > 2) {
        std::cerr << "Invalid number of arguments!" << std::endl;
        return EXIT_FAILURE;
    }

    int flags = 0 | FTW_DEPTH | FTW_MOUNT;
    if (nftw((argc < 2) ? "." : argv[1], saveInStorage, 20, flags) == -1) {
        std::cerr << "nftw error!" << std::endl;
        return EXIT_FAILURE;
    }

    // sort directory names
    std::vector<std::string> dirs;
    for (auto &dir: storage)
        dirs.push_back(dir.first);
    std::sort(dirs.begin(), dirs.end(), comparator);

    for (auto& dirName: dirs) {
        auto dir = storage[dirName];
        std::cout << "\n" << dirName << ":" << std::endl;

        std::sort(dir.begin(), dir.end(),
                [](const file_t& file1, const file_t& file2) {
                    return comparator(file1.first, file2.first);
        });

        for (const auto& file: dir) {
            printFile(file);
        }
    }

    return EXIT_SUCCESS;
}

static int saveInStorage(const char *fpath, const struct stat *sb, int tflag, struct FTW *ftwbuf) {
    std::string path(fpath);
    if (tflag == FTW_D) {
        storage[path].emplace_back(path, *sb);
    }
    else if (tflag == FTW_F) {
        storage[path.substr(0, path.find_last_of("/\\"))].emplace_back(path, *sb);
    }
    return 0;
}

bool comparator(const std::string& str1, const std::string& str2) {

    int i = 0, j = 0;
    while (i < str1.size() && j < str2.size()) {
        if      (tolower(str1.at(i)) < tolower(str2.at(j))) return true;
        else if (tolower(str1.at(i)) > tolower(str2.at(j))) return false;
        ++i; ++j;
    }
    return str1.size() < str2.size();
}

std::string getUserName(uid_t uid)
{
    struct passwd *pw = getpwuid (uid);
    if (pw) {
        return std::string(pw->pw_name);
    }
    return {};
}

bool isExec(struct stat st) {
    return (st.st_mode & S_IEXEC) != 0;
}

void printFile(const file_t& file) {
              //user rwx
    std::cout << ((file.second.st_mode & S_IRUSR) ? "r" : "-")
              << ((file.second.st_mode & S_IWUSR) ? "w" : "-")
              << ((file.second.st_mode & S_IXUSR) ? "x" : "-")
              // group rwx
              << ((file.second.st_mode & S_IRGRP) ? "r" : "-")
              << ((file.second.st_mode & S_IWGRP) ? "w" : "-")
              << ((file.second.st_mode & S_IXGRP) ? "x" : "-")
              // other rwx
              << ((file.second.st_mode & S_IROTH) ? "r" : "-")
              << ((file.second.st_mode & S_IWOTH) ? "w" : "-")
              << ((file.second.st_mode & S_IXOTH) ? "x" : "-");

    // username and size
    std::cout << " " << getUserName(file.second.st_uid) << "\t"
              << std::setw(12)
              << static_cast<intmax_t>(file.second.st_size) << "\t";

    // date and time
    char timeStr[512];
    strftime(timeStr, 512, "%d.%m.%Y %H:%M:%S", localtime(&file.second.st_mtim.tv_sec));
    std::cout << timeStr << "\t";

    // file name
    bool isLink = false;
    std::string fileName;
    // spec files
    if (isExec(file.second) || !S_ISREG(file.second.st_mode)) {
        fileName += ((S_ISDIR(file.second.st_mode)) ? "/" : "");
        fileName += ((S_ISSOCK(file.second.st_mode)) ? "=" : "");
        fileName += ((S_ISFIFO(file.second.st_mode)) ? "|" : "");

        if (S_ISLNK(file.second.st_mode)) {
            fileName += "@";
            isLink = true;
        }

        fileName += ((isExec(file.second)) ? "*" : "");
        fileName += (!fileName.size() ? "?" : "");

    }

    fileName += file.first.substr( file.first.find_last_of("/\\") + 1, file.first.size() - file.first.find_last_of("/\\"));

    if (isLink) {
        char buf[512];
        ssize_t size = readlink(file.first.c_str(), buf, 512);
        buf[size] = '\0';
        fileName += " -> " + std::string(buf);
    }

    std::cout << fileName << std::endl;
}