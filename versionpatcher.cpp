#include <iostream>
#include <fstream>
#include <vector>
#include <mach-o/fat.h>
#include <mach-o/loader.h>
#include <mach-o/arch.h>
#include <arpa/inet.h> // For ntohl

void printCPUType(cpu_type_t cpuType, cpu_subtype_t cpuSubType) {
    std::cout << "CPU Type: ";
    switch (cpuType) {
    case CPU_TYPE_X86:
        std::cout << "x86";
        break;
    case CPU_TYPE_X86_64:
        std::cout << "x86_64";
        break;
    case CPU_TYPE_ARM:
        std::cout << "ARM";
        break;
    case CPU_TYPE_ARM64:
        std::cout << "ARM64";
        break;
    // Add more CPU types as needed
    default:
        std::cout << "Unknown CPU type (" << cpuType << ")";
        break;
    }

    std::cout << ", CPU Subtype: " << cpuSubType << std::endl;
}

void printLoadCommand(uint32_t cmd) {
    switch (cmd) {
    case LC_SEGMENT:
        std::cout << "   LC_SEGMENT" << std::endl;
        break;
    case LC_SYMTAB:
        std::cout << "   LC_SYMTAB" << std::endl;
        break;
    case LC_DYSYMTAB:
        std::cout << "   LC_DYSYMTAB" << std::endl;
        break;
    case LC_LOAD_DYLINKER:
        std::cout << "   LC_LOAD_DYLINKER" << std::endl;
        break;
    case LC_ID_DYLIB:
        std::cout << "   LC_ID_DYLIB" << std::endl;
        break;
    case LC_LOAD_DYLIB:
        std::cout << "   LC_LOAD_DYLIB" << std::endl;
        break;
    case LC_UNIXTHREAD:
        std::cout << "   LC_UNIXTHREAD" << std::endl;
        break;
    // Add more cases as needed
    default:
        std::cout << "   Unknown or unhandled command: " << cmd << std::endl;
        break;
    }
}

void parseFatBinary(const char* path, int major=-1, int minor=0, int patch=0) {
    std::fstream file(path, std::ios::in | std::ios::out | std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file: " << path << std::endl;
        return;
    }

    fat_header fatHeader;
    file.read(reinterpret_cast<char*>(&fatHeader), sizeof(fatHeader));
    if (file.fail() || ntohl(fatHeader.magic) != FAT_MAGIC) {
        std::cerr << "Error: Not a valid fat binary." << std::endl;
        return;
    }

    uint32_t nfat_arch = ntohl(fatHeader.nfat_arch);
    std::cout << "Number of Architectures: " << nfat_arch << std::endl;

    std::vector<fat_arch> archs;
    for (uint32_t i = 0; i < nfat_arch; ++i) {
        fat_arch arch;
        file.read(reinterpret_cast<char*>(&arch), sizeof(arch));
        if (file.fail()) {
            std::cerr << "Error: Failed to read architecture information." << std::endl;
            return;
        }
        archs.push_back(arch);
    }

    int archCount=1;
    for (fat_arch arch : archs) {
        cpu_type_t cpuType = ntohl(arch.cputype);
        cpu_subtype_t cpuSubType = ntohl(arch.cpusubtype);
        std::cout << "Architecture #" << (archCount++) << ":" << std::endl;
        printCPUType(cpuType, cpuSubType);

        file.seekg(ntohl(arch.offset), std::ios::beg); // Return to the start of the header to re-read
        struct mach_header header;
        file.read(reinterpret_cast<char*>(&header), sizeof(header));
        if (file.fail()) {
            std::cerr << " Failed to read Mach-O header." << std::endl;
            return;
        }

        if (header.magic != MH_MAGIC && header.magic != MH_MAGIC_64) {
            std::cerr << " Not a valid Mach-O file." << std::endl;
            return;
        }

        std::cout << " Number of Load Commands: " << header.ncmds << std::endl;

        // Adjust size if this is a 64-bit header
        size_t headerSize = (header.magic == MH_MAGIC_64) ? sizeof(struct mach_header_64) : sizeof(struct mach_header);

        // Move to end of the header structure
        file.seekg(ntohl(arch.offset)+headerSize, std::ios::beg);

        // Parse load commands
        for (uint32_t i = 0; i < header.ncmds; ++i) {
            std::streampos currentInputPos = file.tellg();
            struct load_command cmd;
            file.read(reinterpret_cast<char*>(&cmd), sizeof(cmd));
            if (file.fail()) {
                std::cerr << "  Failed to read load command." << std::endl;
                return;
            }

            if(cmd.cmd==LC_ID_DYLIB)
            {
                std::cout << "   LC_ID_DYLIB found." << std::endl;

                uint32_t nameOffset, timestamp, currentVersion, compatibilityVersion;

                file.read(reinterpret_cast<char*>(&nameOffset), sizeof(nameOffset));
                file.read(reinterpret_cast<char*>(&timestamp), sizeof(timestamp));
                //file.write(reinterpret_cast<char*>(&currentVersion), sizeof(currentVersion));
                file.read(reinterpret_cast<char*>(&currentVersion), sizeof(currentVersion));
                file.read(reinterpret_cast<char*>(&compatibilityVersion), sizeof(compatibilityVersion));

                std::cout << "    timestamp:" << timestamp << " current version:" << currentVersion << " compatibility verison:" << compatibilityVersion << std::endl;

                if(major>=0)
                {
                    file.seekp(static_cast<uint64_t>(currentInputPos)+sizeof(cmd)+8, std::ios::beg); // Seek to current version
                    currentVersion=major << 16 | minor << 8 | patch;
                    file.write(reinterpret_cast<char*>(&currentVersion), sizeof(currentVersion));
                    std::cout << "     version patched." << std::endl;
                }
            }

            file.seekg(static_cast<uint64_t>(currentInputPos)+cmd.cmdsize, std::ios::beg); // Seek to the next command
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <path_to_dylib> [major] [minor] [patch]" << std::endl;
        return 1;
    }

    int major=-1;
    int minor=0;
    int patch=0;
    if (argc > 2) major = std::atoi(argv[2]);
    if (argc > 3) minor = std::atoi(argv[3]);
    if (argc > 4) patch = std::atoi(argv[4]);

    parseFatBinary(argv[1], major, minor, patch);

    return 0;
}
