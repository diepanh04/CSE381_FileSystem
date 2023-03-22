
#include <string>
#include <sstream>
#include <iostream>
#include <array>
#include <memory>
#include <vector>

enum class [[nodiscard]] Status {
    Ok, Error
};

// Size of the drive/array
constexpr int kMaxDriveSize(1200);
// Max filename length in the filesystem
constexpr int kMaxFilenameSize(16);
// Max number of files in the directory
constexpr int kMaxNumberFiles(5);
// The number of bytes each block is on the drive
constexpr int kBlockSize(30);
// The number of elements in the File Allocation Table
constexpr int kTableSize(30);
// The end of file marker in the file allocation table
constexpr int kEndOfFile(-1);


struct FileEntry {
    std::array<char, kMaxFilenameSize> name{ "" };
    int starting_block{ 0 };
    int size{ 0 };
    double last_modified{ 0.0 };
};

struct FATFilesystem {
    // This is the drive we will be reading/writing to/from, and will store our filesystem
    std::array<std::byte, kMaxDriveSize> drive;

    // Memory versions of the root directory/FAT table, use memcpy to save/load to/from the drive
    std::array<FileEntry, kMaxNumberFiles> root_directory_in_memory;
    std::array<int, kTableSize + 10> fat_table_in_memory;
    int current_starting_block = 0;
    std::vector<std::array<int, 2>> block_chunk;

    FATFilesystem() {
        // TODO
        std::memset(drive.data(), 0, kMaxDriveSize);
        for (auto &entry: root_directory_in_memory) {
            entry.name.fill('\0');
            entry.starting_block = kEndOfFile;
            entry.size = 0;
            entry.last_modified = 0.0;
        }
        fat_table_in_memory = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
                                11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 30, 
                                23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 
                                35, 36, 37, 38, 39, 40};  
    }

    Status printFAT() {
        for (int i = 0; i < kTableSize + 10; i++) {
            std:: cout << fat_table_in_memory[i] << " ";
        }
        return Status::Ok;
    }

    Status listDirectory() {
        // TODO
        std::cout << "Filename\tStarting Block\tSize\n";
        std::cout << "--------------------------------------\n";
        for (auto &entry: root_directory_in_memory) {
            if (entry.starting_block != kEndOfFile) {
                std::string filename(entry.name.begin(), entry.name.end());
                std::cout << filename << "\t\t" << entry.starting_block << "\t\t" << entry.size << " bytes\n";
            }
        }
        std::cout << "--------------------------------------\n";
        return Status::Ok;
    }

    Status createFile(
        const std::string& filename,
        unsigned int filesize, std::byte value) {
        // TODO

        // Find an empty entry in the root directory
        int i = 0;
        for (; i < kMaxNumberFiles; i++) {
            if (root_directory_in_memory[i].starting_block == kEndOfFile) {
                break;
            }
        }
        if (i == kMaxNumberFiles) {
            std::cerr << "Error: Root directory is full\n";
            return Status::Error;
        }

        // Find contiguous free blocks in the file allocation table
        // Find the first available block in the FAT
        int start_block = kEndOfFile;
        for (int j = current_starting_block; j < kTableSize; j++) {
            if (fat_table_in_memory[j] != kEndOfFile) {
                start_block = j;
                break;
            }
        }
        std::cout << start_block << std::endl;
        // Update the FAT to mark the blocks as used
        int current_block = start_block;
        int current_size = kBlockSize;
        int next_block = -1;
        while (current_size < filesize && fat_table_in_memory[current_block] != kEndOfFile) {
            next_block = fat_table_in_memory[current_block];
            current_block = next_block;
            current_size += kBlockSize;
            if (current_size > filesize) {
                std::cout << "Cannot find contiguous block for file" << std::endl;
                return Status::Error;
            }
        }
        block_chunk.push_back({start_block, fat_table_in_memory[current_block]});
        fat_table_in_memory[current_block] = kEndOfFile;
        current_starting_block = current_block + 1;

        // create file in root directory
        for (int i = 0; i < kMaxNumberFiles; i++) {
            if (root_directory_in_memory[i].name[0] == '\0') {
                std::copy(filename.begin(), filename.end(), root_directory_in_memory[i].name.begin());
                root_directory_in_memory[i].starting_block = start_block;
                root_directory_in_memory[i].size = filesize;
                root_directory_in_memory[i].last_modified = 0.0;
                break;
            }
        }

        // populate drive
        int current_byte = start_block * kBlockSize;
        for (int i = 0; i < filesize; i++) {
            drive[current_byte] = value;
            current_byte++;
        }

        return Status::Ok;
    }

    Status remove(const std::string& filename) {
        // TODO
        // Locate the file in root directory
        int index = -1;
        for (int i = 0; i < kMaxNumberFiles; i++) {
            if (root_directory_in_memory[i].name.data() == filename) {
                index = i;
                break;
            }
        }
        // File not found
        if (index == -1) {
            std::cout << "Cannot find file " << filename << std::endl;
            return Status::Error;
        }
        // Dellocate memory in FAT
        int current_block = root_directory_in_memory[index].starting_block;
        int next_block = -1;
        while (fat_table_in_memory[current_block] != kEndOfFile) {
            next_block = fat_table_in_memory[current_block];
            current_block = next_block;
        }
        for (int i = 0; i < block_chunk.size(); i++) {
            if (block_chunk.at(i)[0] == root_directory_in_memory[index].starting_block) {
                fat_table_in_memory[current_block] = block_chunk.at(i)[1];
            }
        }
        // Remove file from root directory
        root_directory_in_memory[index] = FileEntry();
        return Status::Ok;
    }

    Status rename(
        const std::string& filename,
        const std::string& new_filename) {
        // TODO
        // Locate the file in root directory
        int index = -1;
        for (int i = 0; i < kMaxNumberFiles; i++) {
            if (root_directory_in_memory[i].name.data() == filename) {
                index = i;
                break;
            }
        }
        // File not found
        if (index == -1) {
            std::cout << "Cannot find file " << filename << std::endl;
            return Status::Error;
        }
        // Rename file
        root_directory_in_memory[index].name = {};
        std::copy(new_filename.begin(), new_filename.end(), root_directory_in_memory[index].name.begin());
        
        return Status::Ok;
    }

    Status print(const std::string& filename) {
        // TODO
        // Locate the file in root directory
        int index = -1;
        for (int i = 0; i < kMaxNumberFiles; i++) {
            if (root_directory_in_memory[i].name.data() == filename) {
                index = i;
                break;
            }
        }
        // File not found
        if (index == -1) {
            std::cout << "Cannot find file " << filename << std::endl;
            return Status::Error;
        }
        FileEntry entry = root_directory_in_memory[index];
        int current_byte = entry.starting_block * kBlockSize;
        for (int i = current_byte; i < entry.size; i++) {
            std::cout << static_cast<char>(drive[current_byte]);
        }
        std::cout << std::endl;
        return Status::Ok;
    }
};

int main() {
    // e.g. to get size of root dir / FAT
    std::cout << sizeof(std::array<FileEntry, kMaxNumberFiles>) << " bytes" << std::endl;
    std::cout << sizeof(std::array<int, kTableSize>) << " bytes" << std::endl;

    // TODO - CONSOLE HERE
    FATFilesystem file_system;
    std::string line;
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, line);
        std::istringstream stream(line);
        std::string command;
        stream >> command;
        Status status = Status::Ok;
        if (command == "ls") {
            status = file_system.listDirectory();
        } else if (command == "create") {
            std::string filename;
            unsigned int filesize;
            char value;
            while (stream >> filename >> filesize >> value) {
                status = file_system.createFile(filename, filesize, static_cast<std::byte>(value));
            }
        } else if (command == "mv") {
            std::string oldfile;
            std::string newfile;
            while (stream >> oldfile >> newfile) {
                status = file_system.rename(oldfile, newfile);
            }
        } else if (command == "rm") {
            std::string filename;
            while (stream >> filename) {
                status = file_system.remove(filename);
            }
        } else if (command == "cat") {
            std::string filename;
            while (stream >> filename) {
                status = file_system.print(filename);
            }
        }
        if (status == Status::Error) {
            std::cout << "Some error has occured" << std::endl;
        }
    }
    

    return 0;
}





