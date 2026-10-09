#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>
#include <filesystem>

#include "GBPokemon.h"
#include "Gen1Pokemon.h"
#include "Gen2Pokemon.h"
#include "Gen3Pokemon.h"

int generation = 2;
Language originLanguage = ENGLISH;
ConversionMethod method = ORIGINAL;

bool printPokemon = false;
bool sanitizeMythicals = true;

int convert(std::string inputPath, std::string outputPath)
{
    std::ifstream input(inputPath, std::ios::binary);
    if (!input)
    {
        std::cerr << "Could not open input file: " << inputPath << '\n';
        return 1;
    }

    std::vector<byte> fileData;
    fileData.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    if (input.bad())
    {
        std::cerr << "Could not read input file: " << inputPath << '\n';
        return 1;
    }

    const std::size_t japaneseSize = generation == 1 ? 59 : 63;
    const std::size_t internationalSize = generation == 1 ? 69 : 73;
    if (fileData.size() != japaneseSize && fileData.size() != internationalSize)
    {
        std::cerr << "Unexpected PK" << generation << " size: " << fileData.size()
                  << " bytes (expected " << japaneseSize << " or " << internationalSize << ")\n";
        return 1;
    }

    const bool japaneseFile = fileData.size() == japaneseSize;
    if ((originLanguage == JAPANESE) != japaneseFile)
    {
        std::cerr << "The selected originLanguage does not match the PK" << generation << " file size\n";
        return 1;
    }

    std::cout << "fileData: " << std::endl;
    for (int i = 0; i < fileData.size(); ++i)
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(fileData[i]) << "";
    std::cout << std::endl;

    const std::size_t partyDataSize = generation == 1 ? 44 : 48;
    const std::size_t pokemonDataOffset = 3;
    const std::size_t trainerNameOffset = pokemonDataOffset + partyDataSize;
    const std::size_t stringSize = japaneseFile ? 6 : 11;
    const std::size_t nicknameOffset = trainerNameOffset + stringSize;

    PokemonTables table;
    std::unique_ptr<GBPokemon> source;
    if (generation == 1)
    {
        source = std::make_unique<Gen1Pokemon>(originLanguage);
    }
    else
    {
        // PCCS conversion only needs the 32-byte boxed portion.
        source = std::make_unique<Gen2Pokemon>(originLanguage);
    }

    const byte* pkFileData = fileData.data();

    std::cout << std::endl <<  "pkFileData: " << std::endl;
    for (int i = 0; i < fileData.size(); ++i)
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(pkFileData[i]) << "";
    std::cout << std::endl << std::endl;

    source->loadData(originLanguage, pkFileData, fileData.size());

    Gen3Pokemon converted;
    if (!source->convertToGen3(&table, &converted, method, sanitizeMythicals))
    {
        std::cerr << "PCCS could not convert the Pokemon (" << inputPath << ")\n\n";
        return 1;
    }

    if (printPokemon)
    {
        converted.print(&table, std::cout);
        std::cout << '\n';
    }

    converted.setChecksum(converted.calculateChecksum());
    converted.updateSecurityData();

    std::ofstream output(outputPath, std::ios::binary);
    if (!output)
    {
        std::cerr << "Could not open output file: " << outputPath << '\n';
        return 1;
    }

    auto data = converted.outputByteArray(false, true);

    output.write(
        reinterpret_cast<const char*>(data.data()),
        converted.dataArraySize
    );
    if (!output)
    {
        std::cerr << "Could not write output file: " << outputPath << '\n';
        return 1;
    }

    std::cout << "Converted " << inputPath << " -> " << outputPath << '\n';
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        std::cerr << "Please specify all necessary arguments." << std::endl;
        return 1;
    }

    std::filesystem::path inFolder = argv[1];
    std::filesystem::path outFolder = argv[2];

    for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(inFolder))
    {
        if (entry.is_regular_file())
        {
            std::string inName = entry.path().filename();
            std::string outName = inName.substr(0, inName.length() - 1) + "3";
            std::cout << "inName: " << inName << std::endl << "outName: " << outName << std::endl;
            convert(inFolder / inName, outFolder / outName);
        }
    }
}