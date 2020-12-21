// Copyright © Kirill Gavrilov, 2020
//
// ddscubemap is a small utility generating a DDS cubemap image from a set of DDS cube sides images.
//
// This code is licensed under MIT license (see LICENSE.txt for details).

#include <iostream>
#include <fstream>
#include <vector>
#include <string>

//! DDS Pixel Format structure.
struct DDSPixelFormat
{
  uint32_t Size;
  uint32_t Flags;
  uint32_t FourCC;
  uint32_t RGBBitCount;
  uint32_t RBitMask;
  uint32_t GBitMask;
  uint32_t BBitMask;
  uint32_t ABitMask;
};

//! DDS File header structure.
struct DDSFileHeader
{
  //! Caps2 flag indicating complete (6 faces) cubemap.
  enum { DDSCompleteCubemap = 0xFE00 };

  //! Return TRUE if cubmap flag is set.
  bool IscompleteCubemap() const { return (Caps2 & DDSFileHeader::DDSCompleteCubemap) != 0; }

  uint32_t Size;
  uint32_t Flags;
  uint32_t Height;
  uint32_t Width;
  uint32_t PitchOrLinearSize;
  uint32_t Depth;
  uint32_t MipMapCount;
  uint32_t Reserved1[11];
  DDSPixelFormat PixelFormatDef;
  uint32_t Caps;
  uint32_t Caps2;
  uint32_t Caps3;
  uint32_t Caps4;
  uint32_t Reserved2;
};

//! Print user help.
static void printHelp()
{
  std::cout << "Usage: ddscubemap PX.dds NX.dds PY.dds NY.dds PZ.dds NZ.dds -o result.dds\n"
               "Created by Kirill Gavrilov <kirill@sview.ru>\n";;
}

int main (int theNbArgs, char** theArgVec)
{
  if (theNbArgs <= 1)
  {
    std::cerr << "Syntax error: wrong number of arguments\n";
    printHelp();
    return 1;
  }

  std::vector<std::string> anInput;
  std::string anOutput;
  for (int anArgIter = 1; anArgIter < theNbArgs; ++anArgIter)
  {
    std::string anArg (theArgVec[anArgIter]);
    if (anArg == "-help"
     || anArg == "--help")
    {
      printHelp();
      return 1;
    }
    else if (anArg == "-o")
    {
      anOutput = theArgVec[++anArgIter];
    }
    else
    {
      anInput.push_back (anArg);
    }
  }
  if (anInput.size() != 6
   || anOutput.empty())
  {
    std::cerr << "Syntax error: wrong number of arguments\n";
    return 1;
  }

  std::ofstream aResFile;
  DDSFileHeader aResHeader;
  std::vector<unsigned char> aBuffer;
  DDSFileHeader anInHeaders[6];
  for (size_t aSide = 0; aSide < 6; ++aSide)
  {
    std::ifstream anInFile;
    anInFile.open (anInput[aSide], std::ios::in | std::ios::binary);
    if (!anInFile)
    {
      std::cerr << "Error: unable to read file '" << anInput[aSide] << "'\n";
      return 1;
    }

    anInFile.seekg (0, std::ios::end);
    const size_t aSize = (size_t )anInFile.tellg();
    aBuffer.resize (aSize, 0);
    anInFile.seekg (0, std::ios::beg);
    anInFile.read ((char* )&aBuffer[0], aSize);
    if (anInFile.gcount() != (std::streamsize )aSize)
    {
      std::cerr << "Error: unable to read file '" << anInput[aSide] << "'\n";
      return 1;
    }

    if (memcmp (&aBuffer[0], "DDS ", 4) != 0
     || aSize < 128)
    {
      std::cerr << "Error: input file '" << anInput[aSide] << "' is not DDS\n";
      return 1;
    }

    anInHeaders[aSide] = *(DDSFileHeader* )&aBuffer[4];
    const DDSFileHeader& aSrcHeader = anInHeaders[aSide];
    if (aSrcHeader.Size != 124
     || aSrcHeader.Width  == 0
     || aSrcHeader.Height == 0
     || aSrcHeader.PixelFormatDef.Size != 32)
    {
      std::cerr << "Error: input file '" << anInput[aSide] << "' is not valid DDS\n";
      return 1;
    }

    char aCompStr[5] = {};
    memcpy (aCompStr, &aSrcHeader.PixelFormatDef.FourCC, 4);

    std::cerr << "Side #" << aSide << " " << aSrcHeader.Width << "x" << aSrcHeader.Height << " Compr: " << aCompStr << " NbMips: " << aSrcHeader.MipMapCount << " \n";
    uint32_t aNbMipComplete = 1;
    const uint32_t aMaxDim = aSrcHeader.Height > aSrcHeader.Width ? aSrcHeader.Height : aSrcHeader.Width;
    for (uint32_t aDimIter = aMaxDim; aDimIter > 1; ++aNbMipComplete, aDimIter /= 2) {}
    if (aNbMipComplete != aSrcHeader.MipMapCount)
    {
      std::cerr << "Warning: incomplete mipmap level set " << aSrcHeader.MipMapCount << " (expected " << aNbMipComplete << ")\n";
    }

    if (aSrcHeader.Width != aSrcHeader.Height)
    {
      std::cerr << "Error: input file '" << anInput[aSide] << "' is not suitable for cubemap\n";
      return 1;
    }

    if (aSide == 0)
    {
      aResHeader = aSrcHeader;
      aResHeader.Caps2 |= DDSFileHeader::DDSCompleteCubemap;
      aResFile.open (anOutput, std::ios::out | std::ios::binary);
      if (!aResFile)
      {
        std::cerr << "Error: unable to write result file '" << anOutput << "'\n";
        return 1;
      }

      aResFile.write ("DDS ", 4);
      aResFile.write ((const char* )&aResHeader, sizeof(aResHeader));
    }
    else
    {
      const DDSFileHeader& aFirstHeader = anInHeaders[0];
      if (aSrcHeader.Width  != aFirstHeader.Width
       || aSrcHeader.Height != aFirstHeader.Height
       || aSrcHeader.PixelFormatDef.FourCC != aFirstHeader.PixelFormatDef.FourCC)
      {
        std::cerr << "Error: input file '" << anInput[aSide] << "' has inconsistent definition\n";
        return 1;
      }
    }

    aResFile.write ((const char* )&aBuffer[128], aSize - 128);
  }
  aResFile.close();
  if (!aResFile.good())
  {
    std::cerr << "Error: unable to write result file '" << anOutput << "'\n";
    return 1;
  }

  return 0;
}
