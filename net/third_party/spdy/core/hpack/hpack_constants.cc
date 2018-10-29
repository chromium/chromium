// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/spdy/core/hpack/hpack_constants.h"

#include <cstddef>
#include <memory>
#include <vector>

#include "base/logging.h"
#include "net/third_party/spdy/core/hpack/hpack_huffman_table.h"
#include "net/third_party/spdy/core/hpack/hpack_static_table.h"
#include "net/third_party/spdy/platform/api/spdy_arraysize.h"

namespace spdy {

// Produced by applying the python program [1] with tables provided by [2]
// (inserted into the source of the python program) and copy-paste them into
// this file.
//
// [1] net/tools/build_hpack_constants.py in Chromium
// [2] http://tools.ietf.org/html/draft-ietf-httpbis-header-compression-08

// HpackHuffmanSymbol entries are initialized as {code, length, id}.
// Codes are specified in the |length| most-significant bits of |code|.
const std::vector<HpackHuffmanSymbol>& HpackHuffmanCodeVector() {
  static const auto* kHpackHuffmanCode = new std::vector<HpackHuffmanSymbol>{
      {0xffc00000ul, 13, 0},    //     11111111|11000
      {0xffffb000ul, 23, 1},    //     11111111|11111111|1011000
      {0xfffffe20ul, 28, 2},    //     11111111|11111111|11111110|0010
      {0xfffffe30ul, 28, 3},    //     11111111|11111111|11111110|0011
      {0xfffffe40ul, 28, 4},    //     11111111|11111111|11111110|0100
      {0xfffffe50ul, 28, 5},    //     11111111|11111111|11111110|0101
      {0xfffffe60ul, 28, 6},    //     11111111|11111111|11111110|0110
      {0xfffffe70ul, 28, 7},    //     11111111|11111111|11111110|0111
      {0xfffffe80ul, 28, 8},    //     11111111|11111111|11111110|1000
      {0xffffea00ul, 24, 9},    //     11111111|11111111|11101010
      {0xfffffff0ul, 30, 10},   //     11111111|11111111|11111111|111100
      {0xfffffe90ul, 28, 11},   //     11111111|11111111|11111110|1001
      {0xfffffea0ul, 28, 12},   //     11111111|11111111|11111110|1010
      {0xfffffff4ul, 30, 13},   //     11111111|11111111|11111111|111101
      {0xfffffeb0ul, 28, 14},   //     11111111|11111111|11111110|1011
      {0xfffffec0ul, 28, 15},   //     11111111|11111111|11111110|1100
      {0xfffffed0ul, 28, 16},   //     11111111|11111111|11111110|1101
      {0xfffffee0ul, 28, 17},   //     11111111|11111111|11111110|1110
      {0xfffffef0ul, 28, 18},   //     11111111|11111111|11111110|1111
      {0xffffff00ul, 28, 19},   //     11111111|11111111|11111111|0000
      {0xffffff10ul, 28, 20},   //     11111111|11111111|11111111|0001
      {0xffffff20ul, 28, 21},   //     11111111|11111111|11111111|0010
      {0xfffffff8ul, 30, 22},   //     11111111|11111111|11111111|111110
      {0xffffff30ul, 28, 23},   //     11111111|11111111|11111111|0011
      {0xffffff40ul, 28, 24},   //     11111111|11111111|11111111|0100
      {0xffffff50ul, 28, 25},   //     11111111|11111111|11111111|0101
      {0xffffff60ul, 28, 26},   //     11111111|11111111|11111111|0110
      {0xffffff70ul, 28, 27},   //     11111111|11111111|11111111|0111
      {0xffffff80ul, 28, 28},   //     11111111|11111111|11111111|1000
      {0xffffff90ul, 28, 29},   //     11111111|11111111|11111111|1001
      {0xffffffa0ul, 28, 30},   //     11111111|11111111|11111111|1010
      {0xffffffb0ul, 28, 31},   //     11111111|11111111|11111111|1011
      {0x50000000ul, 6, 32},    // ' ' 010100
      {0xfe000000ul, 10, 33},   // '!' 11111110|00
      {0xfe400000ul, 10, 34},   // '"' 11111110|01
      {0xffa00000ul, 12, 35},   // '#' 11111111|1010
      {0xffc80000ul, 13, 36},   // '$' 11111111|11001
      {0x54000000ul, 6, 37},    // '%' 010101
      {0xf8000000ul, 8, 38},    // '&' 11111000
      {0xff400000ul, 11, 39},   // ''' 11111111|010
      {0xfe800000ul, 10, 40},   // '(' 11111110|10
      {0xfec00000ul, 10, 41},   // ')' 11111110|11
      {0xf9000000ul, 8, 42},    // '*' 11111001
      {0xff600000ul, 11, 43},   // '+' 11111111|011
      {0xfa000000ul, 8, 44},    // ',' 11111010
      {0x58000000ul, 6, 45},    // '-' 010110
      {0x5c000000ul, 6, 46},    // '.' 010111
      {0x60000000ul, 6, 47},    // '/' 011000
      {0x00000000ul, 5, 48},    // '0' 00000
      {0x08000000ul, 5, 49},    // '1' 00001
      {0x10000000ul, 5, 50},    // '2' 00010
      {0x64000000ul, 6, 51},    // '3' 011001
      {0x68000000ul, 6, 52},    // '4' 011010
      {0x6c000000ul, 6, 53},    // '5' 011011
      {0x70000000ul, 6, 54},    // '6' 011100
      {0x74000000ul, 6, 55},    // '7' 011101
      {0x78000000ul, 6, 56},    // '8' 011110
      {0x7c000000ul, 6, 57},    // '9' 011111
      {0xb8000000ul, 7, 58},    // ':' 1011100
      {0xfb000000ul, 8, 59},    // ';' 11111011
      {0xfff80000ul, 15, 60},   // '<' 11111111|1111100
      {0x80000000ul, 6, 61},    // '=' 100000
      {0xffb00000ul, 12, 62},   // '>' 11111111|1011
      {0xff000000ul, 10, 63},   // '?' 11111111|00
      {0xffd00000ul, 13, 64},   // '@' 11111111|11010
      {0x84000000ul, 6, 65},    // 'A' 100001
      {0xba000000ul, 7, 66},    // 'B' 1011101
      {0xbc000000ul, 7, 67},    // 'C' 1011110
      {0xbe000000ul, 7, 68},    // 'D' 1011111
      {0xc0000000ul, 7, 69},    // 'E' 1100000
      {0xc2000000ul, 7, 70},    // 'F' 1100001
      {0xc4000000ul, 7, 71},    // 'G' 1100010
      {0xc6000000ul, 7, 72},    // 'H' 1100011
      {0xc8000000ul, 7, 73},    // 'I' 1100100
      {0xca000000ul, 7, 74},    // 'J' 1100101
      {0xcc000000ul, 7, 75},    // 'K' 1100110
      {0xce000000ul, 7, 76},    // 'L' 1100111
      {0xd0000000ul, 7, 77},    // 'M' 1101000
      {0xd2000000ul, 7, 78},    // 'N' 1101001
      {0xd4000000ul, 7, 79},    // 'O' 1101010
      {0xd6000000ul, 7, 80},    // 'P' 1101011
      {0xd8000000ul, 7, 81},    // 'Q' 1101100
      {0xda000000ul, 7, 82},    // 'R' 1101101
      {0xdc000000ul, 7, 83},    // 'S' 1101110
      {0xde000000ul, 7, 84},    // 'T' 1101111
      {0xe0000000ul, 7, 85},    // 'U' 1110000
      {0xe2000000ul, 7, 86},    // 'V' 1110001
      {0xe4000000ul, 7, 87},    // 'W' 1110010
      {0xfc000000ul, 8, 88},    // 'X' 11111100
      {0xe6000000ul, 7, 89},    // 'Y' 1110011
      {0xfd000000ul, 8, 90},    // 'Z' 11111101
      {0xffd80000ul, 13, 91},   // '[' 11111111|11011
      {0xfffe0000ul, 19, 92},   // '\' 11111111|11111110|000
      {0xffe00000ul, 13, 93},   // ']' 11111111|11100
      {0xfff00000ul, 14, 94},   // '^' 11111111|111100
      {0x88000000ul, 6, 95},    // '_' 100010
      {0xfffa0000ul, 15, 96},   // '`' 11111111|1111101
      {0x18000000ul, 5, 97},    // 'a' 00011
      {0x8c000000ul, 6, 98},    // 'b' 100011
      {0x20000000ul, 5, 99},    // 'c' 00100
      {0x90000000ul, 6, 100},   // 'd' 100100
      {0x28000000ul, 5, 101},   // 'e' 00101
      {0x94000000ul, 6, 102},   // 'f' 100101
      {0x98000000ul, 6, 103},   // 'g' 100110
      {0x9c000000ul, 6, 104},   // 'h' 100111
      {0x30000000ul, 5, 105},   // 'i' 00110
      {0xe8000000ul, 7, 106},   // 'j' 1110100
      {0xea000000ul, 7, 107},   // 'k' 1110101
      {0xa0000000ul, 6, 108},   // 'l' 101000
      {0xa4000000ul, 6, 109},   // 'm' 101001
      {0xa8000000ul, 6, 110},   // 'n' 101010
      {0x38000000ul, 5, 111},   // 'o' 00111
      {0xac000000ul, 6, 112},   // 'p' 101011
      {0xec000000ul, 7, 113},   // 'q' 1110110
      {0xb0000000ul, 6, 114},   // 'r' 101100
      {0x40000000ul, 5, 115},   // 's' 01000
      {0x48000000ul, 5, 116},   // 't' 01001
      {0xb4000000ul, 6, 117},   // 'u' 101101
      {0xee000000ul, 7, 118},   // 'v' 1110111
      {0xf0000000ul, 7, 119},   // 'w' 1111000
      {0xf2000000ul, 7, 120},   // 'x' 1111001
      {0xf4000000ul, 7, 121},   // 'y' 1111010
      {0xf6000000ul, 7, 122},   // 'z' 1111011
      {0xfffc0000ul, 15, 123},  // '{' 11111111|1111110
      {0xff800000ul, 11, 124},  // '|' 11111111|100
      {0xfff40000ul, 14, 125},  // '}' 11111111|111101
      {0xffe80000ul, 13, 126},  // '~' 11111111|11101
      {0xffffffc0ul, 28, 127},  //     11111111|11111111|11111111|1100
      {0xfffe6000ul, 20, 128},  //     11111111|11111110|0110
      {0xffff4800ul, 22, 129},  //     11111111|11111111|010010
      {0xfffe7000ul, 20, 130},  //     11111111|11111110|0111
      {0xfffe8000ul, 20, 131},  //     11111111|11111110|1000
      {0xffff4c00ul, 22, 132},  //     11111111|11111111|010011
      {0xffff5000ul, 22, 133},  //     11111111|11111111|010100
      {0xffff5400ul, 22, 134},  //     11111111|11111111|010101
      {0xffffb200ul, 23, 135},  //     11111111|11111111|1011001
      {0xffff5800ul, 22, 136},  //     11111111|11111111|010110
      {0xffffb400ul, 23, 137},  //     11111111|11111111|1011010
      {0xffffb600ul, 23, 138},  //     11111111|11111111|1011011
      {0xffffb800ul, 23, 139},  //     11111111|11111111|1011100
      {0xffffba00ul, 23, 140},  //     11111111|11111111|1011101
      {0xffffbc00ul, 23, 141},  //     11111111|11111111|1011110
      {0xffffeb00ul, 24, 142},  //     11111111|11111111|11101011
      {0xffffbe00ul, 23, 143},  //     11111111|11111111|1011111
      {0xffffec00ul, 24, 144},  //     11111111|11111111|11101100
      {0xffffed00ul, 24, 145},  //     11111111|11111111|11101101
      {0xffff5c00ul, 22, 146},  //     11111111|11111111|010111
      {0xffffc000ul, 23, 147},  //     11111111|11111111|1100000
      {0xffffee00ul, 24, 148},  //     11111111|11111111|11101110
      {0xffffc200ul, 23, 149},  //     11111111|11111111|1100001
      {0xffffc400ul, 23, 150},  //     11111111|11111111|1100010
      {0xffffc600ul, 23, 151},  //     11111111|11111111|1100011
      {0xffffc800ul, 23, 152},  //     11111111|11111111|1100100
      {0xfffee000ul, 21, 153},  //     11111111|11111110|11100
      {0xffff6000ul, 22, 154},  //     11111111|11111111|011000
      {0xffffca00ul, 23, 155},  //     11111111|11111111|1100101
      {0xffff6400ul, 22, 156},  //     11111111|11111111|011001
      {0xffffcc00ul, 23, 157},  //     11111111|11111111|1100110
      {0xffffce00ul, 23, 158},  //     11111111|11111111|1100111
      {0xffffef00ul, 24, 159},  //     11111111|11111111|11101111
      {0xffff6800ul, 22, 160},  //     11111111|11111111|011010
      {0xfffee800ul, 21, 161},  //     11111111|11111110|11101
      {0xfffe9000ul, 20, 162},  //     11111111|11111110|1001
      {0xffff6c00ul, 22, 163},  //     11111111|11111111|011011
      {0xffff7000ul, 22, 164},  //     11111111|11111111|011100
      {0xffffd000ul, 23, 165},  //     11111111|11111111|1101000
      {0xffffd200ul, 23, 166},  //     11111111|11111111|1101001
      {0xfffef000ul, 21, 167},  //     11111111|11111110|11110
      {0xffffd400ul, 23, 168},  //     11111111|11111111|1101010
      {0xffff7400ul, 22, 169},  //     11111111|11111111|011101
      {0xffff7800ul, 22, 170},  //     11111111|11111111|011110
      {0xfffff000ul, 24, 171},  //     11111111|11111111|11110000
      {0xfffef800ul, 21, 172},  //     11111111|11111110|11111
      {0xffff7c00ul, 22, 173},  //     11111111|11111111|011111
      {0xffffd600ul, 23, 174},  //     11111111|11111111|1101011
      {0xffffd800ul, 23, 175},  //     11111111|11111111|1101100
      {0xffff0000ul, 21, 176},  //     11111111|11111111|00000
      {0xffff0800ul, 21, 177},  //     11111111|11111111|00001
      {0xffff8000ul, 22, 178},  //     11111111|11111111|100000
      {0xffff1000ul, 21, 179},  //     11111111|11111111|00010
      {0xffffda00ul, 23, 180},  //     11111111|11111111|1101101
      {0xffff8400ul, 22, 181},  //     11111111|11111111|100001
      {0xffffdc00ul, 23, 182},  //     11111111|11111111|1101110
      {0xffffde00ul, 23, 183},  //     11111111|11111111|1101111
      {0xfffea000ul, 20, 184},  //     11111111|11111110|1010
      {0xffff8800ul, 22, 185},  //     11111111|11111111|100010
      {0xffff8c00ul, 22, 186},  //     11111111|11111111|100011
      {0xffff9000ul, 22, 187},  //     11111111|11111111|100100
      {0xffffe000ul, 23, 188},  //     11111111|11111111|1110000
      {0xffff9400ul, 22, 189},  //     11111111|11111111|100101
      {0xffff9800ul, 22, 190},  //     11111111|11111111|100110
      {0xffffe200ul, 23, 191},  //     11111111|11111111|1110001
      {0xfffff800ul, 26, 192},  //     11111111|11111111|11111000|00
      {0xfffff840ul, 26, 193},  //     11111111|11111111|11111000|01
      {0xfffeb000ul, 20, 194},  //     11111111|11111110|1011
      {0xfffe2000ul, 19, 195},  //     11111111|11111110|001
      {0xffff9c00ul, 22, 196},  //     11111111|11111111|100111
      {0xffffe400ul, 23, 197},  //     11111111|11111111|1110010
      {0xffffa000ul, 22, 198},  //     11111111|11111111|101000
      {0xfffff600ul, 25, 199},  //     11111111|11111111|11110110|0
      {0xfffff880ul, 26, 200},  //     11111111|11111111|11111000|10
      {0xfffff8c0ul, 26, 201},  //     11111111|11111111|11111000|11
      {0xfffff900ul, 26, 202},  //     11111111|11111111|11111001|00
      {0xfffffbc0ul, 27, 203},  //     11111111|11111111|11111011|110
      {0xfffffbe0ul, 27, 204},  //     11111111|11111111|11111011|111
      {0xfffff940ul, 26, 205},  //     11111111|11111111|11111001|01
      {0xfffff100ul, 24, 206},  //     11111111|11111111|11110001
      {0xfffff680ul, 25, 207},  //     11111111|11111111|11110110|1
      {0xfffe4000ul, 19, 208},  //     11111111|11111110|010
      {0xffff1800ul, 21, 209},  //     11111111|11111111|00011
      {0xfffff980ul, 26, 210},  //     11111111|11111111|11111001|10
      {0xfffffc00ul, 27, 211},  //     11111111|11111111|11111100|000
      {0xfffffc20ul, 27, 212},  //     11111111|11111111|11111100|001
      {0xfffff9c0ul, 26, 213},  //     11111111|11111111|11111001|11
      {0xfffffc40ul, 27, 214},  //     11111111|11111111|11111100|010
      {0xfffff200ul, 24, 215},  //     11111111|11111111|11110010
      {0xffff2000ul, 21, 216},  //     11111111|11111111|00100
      {0xffff2800ul, 21, 217},  //     11111111|11111111|00101
      {0xfffffa00ul, 26, 218},  //     11111111|11111111|11111010|00
      {0xfffffa40ul, 26, 219},  //     11111111|11111111|11111010|01
      {0xffffffd0ul, 28, 220},  //     11111111|11111111|11111111|1101
      {0xfffffc60ul, 27, 221},  //     11111111|11111111|11111100|011
      {0xfffffc80ul, 27, 222},  //     11111111|11111111|11111100|100
      {0xfffffca0ul, 27, 223},  //     11111111|11111111|11111100|101
      {0xfffec000ul, 20, 224},  //     11111111|11111110|1100
      {0xfffff300ul, 24, 225},  //     11111111|11111111|11110011
      {0xfffed000ul, 20, 226},  //     11111111|11111110|1101
      {0xffff3000ul, 21, 227},  //     11111111|11111111|00110
      {0xffffa400ul, 22, 228},  //     11111111|11111111|101001
      {0xffff3800ul, 21, 229},  //     11111111|11111111|00111
      {0xffff4000ul, 21, 230},  //     11111111|11111111|01000
      {0xffffe600ul, 23, 231},  //     11111111|11111111|1110011
      {0xffffa800ul, 22, 232},  //     11111111|11111111|101010
      {0xffffac00ul, 22, 233},  //     11111111|11111111|101011
      {0xfffff700ul, 25, 234},  //     11111111|11111111|11110111|0
      {0xfffff780ul, 25, 235},  //     11111111|11111111|11110111|1
      {0xfffff400ul, 24, 236},  //     11111111|11111111|11110100
      {0xfffff500ul, 24, 237},  //     11111111|11111111|11110101
      {0xfffffa80ul, 26, 238},  //     11111111|11111111|11111010|10
      {0xffffe800ul, 23, 239},  //     11111111|11111111|1110100
      {0xfffffac0ul, 26, 240},  //     11111111|11111111|11111010|11
      {0xfffffcc0ul, 27, 241},  //     11111111|11111111|11111100|110
      {0xfffffb00ul, 26, 242},  //     11111111|11111111|11111011|00
      {0xfffffb40ul, 26, 243},  //     11111111|11111111|11111011|01
      {0xfffffce0ul, 27, 244},  //     11111111|11111111|11111100|111
      {0xfffffd00ul, 27, 245},  //     11111111|11111111|11111101|000
      {0xfffffd20ul, 27, 246},  //     11111111|11111111|11111101|001
      {0xfffffd40ul, 27, 247},  //     11111111|11111111|11111101|010
      {0xfffffd60ul, 27, 248},  //     11111111|11111111|11111101|011
      {0xffffffe0ul, 28, 249},  //     11111111|11111111|11111111|1110
      {0xfffffd80ul, 27, 250},  //     11111111|11111111|11111101|100
      {0xfffffda0ul, 27, 251},  //     11111111|11111111|11111101|101
      {0xfffffdc0ul, 27, 252},  //     11111111|11111111|11111101|110
      {0xfffffde0ul, 27, 253},  //     11111111|11111111|11111101|111
      {0xfffffe00ul, 27, 254},  //     11111111|11111111|11111110|000
      {0xfffffb80ul, 26, 255},  //     11111111|11111111|11111011|10
      {0xfffffffcul, 30, 256},  // EOS 11111111|11111111|11111111|111111
  };
  return *kHpackHuffmanCode;
}

// The "constructor" for a HpackStaticEntry that computes the lengths at
// compile time.
#define STATIC_ENTRY(name, value) \
  { name, SPDY_ARRAYSIZE(name) - 1, value, SPDY_ARRAYSIZE(value) - 1 }

const std::vector<HpackStaticEntry>& HpackStaticTableVector() {
  static const auto* kHpackStaticTable = new std::vector<HpackStaticEntry>{
      STATIC_ENTRY(":authority", ""),                    // 1
      STATIC_ENTRY(":method", "GET"),                    // 2
      STATIC_ENTRY(":method", "POST"),                   // 3
      STATIC_ENTRY(":path", "/"),                        // 4
      STATIC_ENTRY(":path", "/index.html"),              // 5
      STATIC_ENTRY(":scheme", "http"),                   // 6
      STATIC_ENTRY(":scheme", "https"),                  // 7
      STATIC_ENTRY(":status", "200"),                    // 8
      STATIC_ENTRY(":status", "204"),                    // 9
      STATIC_ENTRY(":status", "206"),                    // 10
      STATIC_ENTRY(":status", "304"),                    // 11
      STATIC_ENTRY(":status", "400"),                    // 12
      STATIC_ENTRY(":status", "404"),                    // 13
      STATIC_ENTRY(":status", "500"),                    // 14
      STATIC_ENTRY("accept-charset", ""),                // 15
      STATIC_ENTRY("accept-encoding", "gzip, deflate"),  // 16
      STATIC_ENTRY("accept-language", ""),               // 17
      STATIC_ENTRY("accept-ranges", ""),                 // 18
      STATIC_ENTRY("accept", ""),                        // 19
      STATIC_ENTRY("access-control-allow-origin", ""),   // 20
      STATIC_ENTRY("age", ""),                           // 21
      STATIC_ENTRY("allow", ""),                         // 22
      STATIC_ENTRY("authorization", ""),                 // 23
      STATIC_ENTRY("cache-control", ""),                 // 24
      STATIC_ENTRY("content-disposition", ""),           // 25
      STATIC_ENTRY("content-encoding", ""),              // 26
      STATIC_ENTRY("content-language", ""),              // 27
      STATIC_ENTRY("content-length", ""),                // 28
      STATIC_ENTRY("content-location", ""),              // 29
      STATIC_ENTRY("content-range", ""),                 // 30
      STATIC_ENTRY("content-type", ""),                  // 31
      STATIC_ENTRY("cookie", ""),                        // 32
      STATIC_ENTRY("date", ""),                          // 33
      STATIC_ENTRY("etag", ""),                          // 34
      STATIC_ENTRY("expect", ""),                        // 35
      STATIC_ENTRY("expires", ""),                       // 36
      STATIC_ENTRY("from", ""),                          // 37
      STATIC_ENTRY("host", ""),                          // 38
      STATIC_ENTRY("if-match", ""),                      // 39
      STATIC_ENTRY("if-modified-since", ""),             // 40
      STATIC_ENTRY("if-none-match", ""),                 // 41
      STATIC_ENTRY("if-range", ""),                      // 42
      STATIC_ENTRY("if-unmodified-since", ""),           // 43
      STATIC_ENTRY("last-modified", ""),                 // 44
      STATIC_ENTRY("link", ""),                          // 45
      STATIC_ENTRY("location", ""),                      // 46
      STATIC_ENTRY("max-forwards", ""),                  // 47
      STATIC_ENTRY("proxy-authenticate", ""),            // 48
      STATIC_ENTRY("proxy-authorization", ""),           // 49
      STATIC_ENTRY("range", ""),                         // 50
      STATIC_ENTRY("referer", ""),                       // 51
      STATIC_ENTRY("refresh", ""),                       // 52
      STATIC_ENTRY("retry-after", ""),                   // 53
      STATIC_ENTRY("server", ""),                        // 54
      STATIC_ENTRY("set-cookie", ""),                    // 55
      STATIC_ENTRY("strict-transport-security", ""),     // 56
      STATIC_ENTRY("transfer-encoding", ""),             // 57
      STATIC_ENTRY("user-agent", ""),                    // 58
      STATIC_ENTRY("vary", ""),                          // 59
      STATIC_ENTRY("via", ""),                           // 60
      STATIC_ENTRY("www-authenticate", ""),              // 61
  };
  return *kHpackStaticTable;
}

#undef STATIC_ENTRY

const HpackHuffmanTable& ObtainHpackHuffmanTable() {
  static const HpackHuffmanTable* const shared_huffman_table = []() {
    auto* table = new HpackHuffmanTable();
    CHECK(table->Initialize(HpackHuffmanCodeVector().data(),
                            HpackHuffmanCodeVector().size()));
    CHECK(table->IsInitialized());
    return table;
  }();
  return *shared_huffman_table;
}

const HpackStaticTable& ObtainHpackStaticTable() {
  static const HpackStaticTable* const shared_static_table = []() {
    auto* table = new HpackStaticTable();
    table->Initialize(HpackStaticTableVector().data(),
                      HpackStaticTableVector().size());
    CHECK(table->IsInitialized());
    return table;
  }();
  return *shared_static_table;
}

}  // namespace spdy
