// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/parsers/vp9_uncompressed_header_parser.h"

#include <type_traits>

#include "base/logging.h"

namespace media {

namespace {

// 10.5 Default probability tables
Vp9FrameContext kVp9DefaultFrameContext = {
    // tx_probs_8x8
    {{100}, {66}},
    // tx_probs_16x16
    {{20, 152}, {15, 101}},
    // tx_probs_32x32
    {{3, 136, 37}, {5, 52, 13}},
    // coef_probs
    {// 4x4
     {{{{{195, 29, 183}, {84, 49, 136}, {8, 42, 71}},
        {{31, 107, 169},
         {35, 99, 159},
         {17, 82, 140},
         {8, 66, 114},
         {2, 44, 76},
         {1, 19, 32}},
        {{40, 132, 201},
         {29, 114, 187},
         {13, 91, 157},
         {7, 75, 127},
         {3, 58, 95},
         {1, 28, 47}},
        {{69, 142, 221},
         {42, 122, 201},
         {15, 91, 159},
         {6, 67, 121},
         {1, 42, 77},
         {1, 17, 31}},
        {{102, 148, 228},
         {67, 117, 204},
         {17, 82, 154},
         {6, 59, 114},
         {2, 39, 75},
         {1, 15, 29}},
        {{156, 57, 233},
         {119, 57, 212},
         {58, 48, 163},
         {29, 40, 124},
         {12, 30, 81},
         {3, 12, 31}}},
       {{{191, 107, 226}, {124, 117, 204}, {25, 99, 155}},
        {{29, 148, 210},
         {37, 126, 194},
         {8, 93, 157},
         {2, 68, 118},
         {1, 39, 69},
         {1, 17, 33}},
        {{41, 151, 213},
         {27, 123, 193},
         {3, 82, 144},
         {1, 58, 105},
         {1, 32, 60},
         {1, 13, 26}},
        {{59, 159, 220},
         {23, 126, 198},
         {4, 88, 151},
         {1, 66, 114},
         {1, 38, 71},
         {1, 18, 34}},
        {{114, 136, 232},
         {51, 114, 207},
         {11, 83, 155},
         {3, 56, 105},
         {1, 33, 65},
         {1, 17, 34}},
        {{149, 65, 234},
         {121, 57, 215},
         {61, 49, 166},
         {28, 36, 114},
         {12, 25, 76},
         {3, 16, 42}}}},
      {{{{214, 49, 220}, {132, 63, 188}, {42, 65, 137}},
        {{85, 137, 221},
         {104, 131, 216},
         {49, 111, 192},
         {21, 87, 155},
         {2, 49, 87},
         {1, 16, 28}},
        {{89, 163, 230},
         {90, 137, 220},
         {29, 100, 183},
         {10, 70, 135},
         {2, 42, 81},
         {1, 17, 33}},
        {{108, 167, 237},
         {55, 133, 222},
         {15, 97, 179},
         {4, 72, 135},
         {1, 45, 85},
         {1, 19, 38}},
        {{124, 146, 240},
         {66, 124, 224},
         {17, 88, 175},
         {4, 58, 122},
         {1, 36, 75},
         {1, 18, 37}},
        {{141, 79, 241},
         {126, 70, 227},
         {66, 58, 182},
         {30, 44, 136},
         {12, 34, 96},
         {2, 20, 47}}},
       {{{229, 99, 249}, {143, 111, 235}, {46, 109, 192}},
        {{82, 158, 236},
         {94, 146, 224},
         {25, 117, 191},
         {9, 87, 149},
         {3, 56, 99},
         {1, 33, 57}},
        {{83, 167, 237},
         {68, 145, 222},
         {10, 103, 177},
         {2, 72, 131},
         {1, 41, 79},
         {1, 20, 39}},
        {{99, 167, 239},
         {47, 141, 224},
         {10, 104, 178},
         {2, 73, 133},
         {1, 44, 85},
         {1, 22, 47}},
        {{127, 145, 243},
         {71, 129, 228},
         {17, 93, 177},
         {3, 61, 124},
         {1, 41, 84},
         {1, 21, 52}},
        {{157, 78, 244},
         {140, 72, 231},
         {69, 58, 184},
         {31, 44, 137},
         {14, 38, 105},
         {8, 23, 61}}}}},
     // 8x8
     {{{{{125, 34, 187}, {52, 41, 133}, {6, 31, 56}},
        {{37, 109, 153},
         {51, 102, 147},
         {23, 87, 128},
         {8, 67, 101},
         {1, 41, 63},
         {1, 19, 29}},
        {{31, 154, 185},
         {17, 127, 175},
         {6, 96, 145},
         {2, 73, 114},
         {1, 51, 82},
         {1, 28, 45}},
        {{23, 163, 200},
         {10, 131, 185},
         {2, 93, 148},
         {1, 67, 111},
         {1, 41, 69},
         {1, 14, 24}},
        {{29, 176, 217},
         {12, 145, 201},
         {3, 101, 156},
         {1, 69, 111},
         {1, 39, 63},
         {1, 14, 23}},
        {{57, 192, 233},
         {25, 154, 215},
         {6, 109, 167},
         {3, 78, 118},
         {1, 48, 69},
         {1, 21, 29}}},
       {{{202, 105, 245}, {108, 106, 216}, {18, 90, 144}},
        {{33, 172, 219},
         {64, 149, 206},
         {14, 117, 177},
         {5, 90, 141},
         {2, 61, 95},
         {1, 37, 57}},
        {{33, 179, 220},
         {11, 140, 198},
         {1, 89, 148},
         {1, 60, 104},
         {1, 33, 57},
         {1, 12, 21}},
        {{30, 181, 221},
         {8, 141, 198},
         {1, 87, 145},
         {1, 58, 100},
         {1, 31, 55},
         {1, 12, 20}},
        {{32, 186, 224},
         {7, 142, 198},
         {1, 86, 143},
         {1, 58, 100},
         {1, 31, 55},
         {1, 12, 22}},
        {{57, 192, 227},
         {20, 143, 204},
         {3, 96, 154},
         {1, 68, 112},
         {1, 42, 69},
         {1, 19, 32}}}},
      {{{{212, 35, 215}, {113, 47, 169}, {29, 48, 105}},
        {{74, 129, 203},
         {106, 120, 203},
         {49, 107, 178},
         {19, 84, 144},
         {4, 50, 84},
         {1, 15, 25}},
        {{71, 172, 217},
         {44, 141, 209},
         {15, 102, 173},
         {6, 76, 133},
         {2, 51, 89},
         {1, 24, 42}},
        {{64, 185, 231},
         {31, 148, 216},
         {8, 103, 175},
         {3, 74, 131},
         {1, 46, 81},
         {1, 18, 30}},
        {{65, 196, 235},
         {25, 157, 221},
         {5, 105, 174},
         {1, 67, 120},
         {1, 38, 69},
         {1, 15, 30}},
        {{65, 204, 238},
         {30, 156, 224},
         {7, 107, 177},
         {2, 70, 124},
         {1, 42, 73},
         {1, 18, 34}}},
       {{{225, 86, 251}, {144, 104, 235}, {42, 99, 181}},
        {{85, 175, 239},
         {112, 165, 229},
         {29, 136, 200},
         {12, 103, 162},
         {6, 77, 123},
         {2, 53, 84}},
        {{75, 183, 239},
         {30, 155, 221},
         {3, 106, 171},
         {1, 74, 128},
         {1, 44, 76},
         {1, 17, 28}},
        {{73, 185, 240},
         {27, 159, 222},
         {2, 107, 172},
         {1, 75, 127},
         {1, 42, 73},
         {1, 17, 29}},
        {{62, 190, 238},
         {21, 159, 222},
         {2, 107, 172},
         {1, 72, 122},
         {1, 40, 71},
         {1, 18, 32}},
        {{61, 199, 240},
         {27, 161, 226},
         {4, 113, 180},
         {1, 76, 129},
         {1, 46, 80},
         {1, 23, 41}}}}},
     // 16x16
     {{{{{7, 27, 153}, {5, 30, 95}, {1, 16, 30}},
        {{50, 75, 127},
         {57, 75, 124},
         {27, 67, 108},
         {10, 54, 86},
         {1, 33, 52},
         {1, 12, 18}},
        {{43, 125, 151},
         {26, 108, 148},
         {7, 83, 122},
         {2, 59, 89},
         {1, 38, 60},
         {1, 17, 27}},
        {{23, 144, 163},
         {13, 112, 154},
         {2, 75, 117},
         {1, 50, 81},
         {1, 31, 51},
         {1, 14, 23}},
        {{18, 162, 185},
         {6, 123, 171},
         {1, 78, 125},
         {1, 51, 86},
         {1, 31, 54},
         {1, 14, 23}},
        {{15, 199, 227},
         {3, 150, 204},
         {1, 91, 146},
         {1, 55, 95},
         {1, 30, 53},
         {1, 11, 20}}},
       {{{19, 55, 240}, {19, 59, 196}, {3, 52, 105}},
        {{41, 166, 207},
         {104, 153, 199},
         {31, 123, 181},
         {14, 101, 152},
         {5, 72, 106},
         {1, 36, 52}},
        {{35, 176, 211},
         {12, 131, 190},
         {2, 88, 144},
         {1, 60, 101},
         {1, 36, 60},
         {1, 16, 28}},
        {{28, 183, 213},
         {8, 134, 191},
         {1, 86, 142},
         {1, 56, 96},
         {1, 30, 53},
         {1, 12, 20}},
        {{20, 190, 215},
         {4, 135, 192},
         {1, 84, 139},
         {1, 53, 91},
         {1, 28, 49},
         {1, 11, 20}},
        {{13, 196, 216},
         {2, 137, 192},
         {1, 86, 143},
         {1, 57, 99},
         {1, 32, 56},
         {1, 13, 24}}}},
      {{{{211, 29, 217}, {96, 47, 156}, {22, 43, 87}},
        {{78, 120, 193},
         {111, 116, 186},
         {46, 102, 164},
         {15, 80, 128},
         {2, 49, 76},
         {1, 18, 28}},
        {{71, 161, 203},
         {42, 132, 192},
         {10, 98, 150},
         {3, 69, 109},
         {1, 44, 70},
         {1, 18, 29}},
        {{57, 186, 211},
         {30, 140, 196},
         {4, 93, 146},
         {1, 62, 102},
         {1, 38, 65},
         {1, 16, 27}},
        {{47, 199, 217},
         {14, 145, 196},
         {1, 88, 142},
         {1, 57, 98},
         {1, 36, 62},
         {1, 15, 26}},
        {{26, 219, 229},
         {5, 155, 207},
         {1, 94, 151},
         {1, 60, 104},
         {1, 36, 62},
         {1, 16, 28}}},
       {{{233, 29, 248}, {146, 47, 220}, {43, 52, 140}},
        {{100, 163, 232},
         {179, 161, 222},
         {63, 142, 204},
         {37, 113, 174},
         {26, 89, 137},
         {18, 68, 97}},
        {{85, 181, 230},
         {32, 146, 209},
         {7, 100, 164},
         {3, 71, 121},
         {1, 45, 77},
         {1, 18, 30}},
        {{65, 187, 230},
         {20, 148, 207},
         {2, 97, 159},
         {1, 68, 116},
         {1, 40, 70},
         {1, 14, 29}},
        {{40, 194, 227},
         {8, 147, 204},
         {1, 94, 155},
         {1, 65, 112},
         {1, 39, 66},
         {1, 14, 26}},
        {{16, 208, 228},
         {3, 151, 207},
         {1, 98, 160},
         {1, 67, 117},
         {1, 41, 74},
         {1, 17, 31}}}}},
     // 32x32
     {{{{{17, 38, 140}, {7, 34, 80}, {1, 17, 29}},
        {{37, 75, 128},
         {41, 76, 128},
         {26, 66, 116},
         {12, 52, 94},
         {2, 32, 55},
         {1, 10, 16}},
        {{50, 127, 154},
         {37, 109, 152},
         {16, 82, 121},
         {5, 59, 85},
         {1, 35, 54},
         {1, 13, 20}},
        {{40, 142, 167},
         {17, 110, 157},
         {2, 71, 112},
         {1, 44, 72},
         {1, 27, 45},
         {1, 11, 17}},
        {{30, 175, 188},
         {9, 124, 169},
         {1, 74, 116},
         {1, 48, 78},
         {1, 30, 49},
         {1, 11, 18}},
        {{10, 222, 223},
         {2, 150, 194},
         {1, 83, 128},
         {1, 48, 79},
         {1, 27, 45},
         {1, 11, 17}}},
       {{{36, 41, 235}, {29, 36, 193}, {10, 27, 111}},
        {{85, 165, 222},
         {177, 162, 215},
         {110, 135, 195},
         {57, 113, 168},
         {23, 83, 120},
         {10, 49, 61}},
        {{85, 190, 223},
         {36, 139, 200},
         {5, 90, 146},
         {1, 60, 103},
         {1, 38, 65},
         {1, 18, 30}},
        {{72, 202, 223},
         {23, 141, 199},
         {2, 86, 140},
         {1, 56, 97},
         {1, 36, 61},
         {1, 16, 27}},
        {{55, 218, 225},
         {13, 145, 200},
         {1, 86, 141},
         {1, 57, 99},
         {1, 35, 61},
         {1, 13, 22}},
        {{15, 235, 212},
         {1, 132, 184},
         {1, 84, 139},
         {1, 57, 97},
         {1, 34, 56},
         {1, 14, 23}}}},
      {{{{181, 21, 201}, {61, 37, 123}, {10, 38, 71}},
        {{47, 106, 172},
         {95, 104, 173},
         {42, 93, 159},
         {18, 77, 131},
         {4, 50, 81},
         {1, 17, 23}},
        {{62, 147, 199},
         {44, 130, 189},
         {28, 102, 154},
         {18, 75, 115},
         {2, 44, 65},
         {1, 12, 19}},
        {{55, 153, 210},
         {24, 130, 194},
         {3, 93, 146},
         {1, 61, 97},
         {1, 31, 50},
         {1, 10, 16}},
        {{49, 186, 223},
         {17, 148, 204},
         {1, 96, 142},
         {1, 53, 83},
         {1, 26, 44},
         {1, 11, 17}},
        {{13, 217, 212},
         {2, 136, 180},
         {1, 78, 124},
         {1, 50, 83},
         {1, 29, 49},
         {1, 14, 23}}},
       {{{197, 13, 247}, {82, 17, 222}, {25, 17, 162}},
        {{126, 186, 247},
         {234, 191, 243},
         {176, 177, 234},
         {104, 158, 220},
         {66, 128, 186},
         {55, 90, 137}},
        {{111, 197, 242},
         {46, 158, 219},
         {9, 104, 171},
         {2, 65, 125},
         {1, 44, 80},
         {1, 17, 91}},
        {{104, 208, 245},
         {39, 168, 224},
         {3, 109, 162},
         {1, 79, 124},
         {1, 50, 102},
         {1, 43, 102}},
        {{84, 220, 246},
         {31, 177, 231},
         {2, 115, 180},
         {1, 79, 134},
         {1, 55, 77},
         {1, 60, 79}},
        {{43, 243, 240},
         {8, 180, 217},
         {1, 115, 166},
         {1, 84, 121},
         {1, 51, 67},
         {1, 16, 6}}}}}},
    // skip_prob
    {192, 128, 64},
    // inter_mode_probs
    {{2, 173, 34},
     {7, 145, 85},
     {7, 166, 63},
     {7, 94, 66},
     {8, 64, 46},
     {17, 81, 31},
     {25, 29, 30}},
    // interp_filter_probs
    {{235, 162}, {36, 255}, {34, 3}, {149, 144}},
    // is_inter_prob
    {9, 102, 187, 225},
    // comp_mode_prob
    {239, 183, 119, 96, 41},
    // single_ref_prob
    {{33, 16}, {77, 74}, {142, 142}, {172, 170}, {238, 247}},
    // comp_ref_prob
    {50, 126, 123, 221, 226},
    // y_mode_probs
    {{65, 32, 18, 144, 162, 194, 41, 51, 98},
     {132, 68, 18, 165, 217, 196, 45, 40, 78},
     {173, 80, 19, 176, 240, 193, 64, 35, 46},
     {221, 135, 38, 194, 248, 121, 96, 85, 29}},
    // uv_mode_probs
    {{120, 7, 76, 176, 208, 126, 28, 54, 103},
     {48, 12, 154, 155, 139, 90, 34, 117, 119},
     {67, 6, 25, 204, 243, 158, 13, 21, 96},
     {97, 5, 44, 131, 176, 139, 48, 68, 97},
     {83, 5, 42, 156, 111, 152, 26, 49, 152},
     {80, 5, 58, 178, 74, 83, 33, 62, 145},
     {86, 5, 32, 154, 192, 168, 14, 22, 163},
     {85, 5, 32, 156, 216, 148, 19, 29, 73},
     {77, 7, 64, 116, 132, 122, 37, 126, 120},
     {101, 21, 107, 181, 192, 103, 19, 67, 125}},
    // partition_probs
    {{199, 122, 141},
     {147, 63, 159},
     {148, 133, 118},
     {121, 104, 114},
     {174, 73, 87},
     {92, 41, 83},
     {82, 99, 50},
     {53, 39, 39},
     {177, 58, 59},
     {68, 26, 63},
     {52, 79, 25},
     {17, 14, 12},
     {222, 34, 30},
     {72, 16, 44},
     {58, 32, 12},
     {10, 7, 6}},
    // mv_joint_probs
    {32, 64, 96},
    // mv_sign_prob
    {128, 128},
    // mv_class_probs
    {{224, 144, 192, 168, 192, 176, 192, 198, 198, 245},
     {216, 128, 176, 160, 176, 176, 192, 198, 198, 208}},
    // mv_class0_bit_prob
    {216, 208},
    // mv_bits_prob
    {{136, 140, 148, 160, 176, 192, 224, 234, 234, 240},
     {136, 140, 148, 160, 176, 192, 224, 234, 234, 240}},
    // mv_class0_fr_probs
    {{{128, 128, 64}, {96, 112, 64}}, {{128, 128, 64}, {96, 112, 64}}},
    // mv_fr_probs
    {{64, 96, 64}, {64, 96, 64}},
    // mv_class0_hp_prob
    {160, 160},
    // mv_hp_prob
    {128, 128},
};

// Helper function for Vp9Parser::ReadTileInfo. Defined as
// calc_min_log2_tile_cols in spec 6.2.14 Tile size calculation.
int GetMinLog2TileCols(int sb64_cols) {
  const int kMaxTileWidthB64 = 64;
  int min_log2 = 0;
  while ((kMaxTileWidthB64 << min_log2) < sb64_cols)
    min_log2++;
  return min_log2;
}

// Helper function for Vp9Parser::ReadTileInfo. Defined as
// calc_max_log2_tile_cols in spec 6.2.14 Tile size calculation.
int GetMaxLog2TileCols(int sb64_cols) {
  const int kMinTileWidthB64 = 4;
  int max_log2 = 1;
  while ((sb64_cols >> max_log2) >= kMinTileWidthB64)
    max_log2++;
  return max_log2 - 1;
}

}  // namespace

Vp9UncompressedHeaderParser::Vp9UncompressedHeaderParser(
    Vp9Parser::Context* context)
    : context_(context) {}

const Vp9FrameContext&
Vp9UncompressedHeaderParser::GetVp9DefaultFrameContextForTesting() const {
  return kVp9DefaultFrameContext;
}

uint8_t Vp9UncompressedHeaderParser::ReadProfile() {
  uint8_t profile = 0;

  // LSB first.
  if (reader_.ReadBool())
    profile |= 1;
  if (reader_.ReadBool())
    profile |= 2;
  if (profile > 2 && reader_.ReadBool())
    profile += 1;
  return profile;
}

// 6.2.1 Frame sync syntax
bool Vp9UncompressedHeaderParser::VerifySyncCode() {
  const int kSyncCode = 0x498342;
  if (reader_.ReadLiteral(8 * 3) != kSyncCode) {
    DVLOG(1) << "Invalid frame sync code";
    return false;
  }
  return true;
}

// 6.2.2 Color config syntax
bool Vp9UncompressedHeaderParser::ReadColorConfig(Vp9FrameHeader* fhdr) {
  if (fhdr->profile == 2 || fhdr->profile == 3) {
    fhdr->bit_depth = reader_.ReadBool() ? 12 : 10;
  } else {
    fhdr->bit_depth = 8;
  }

  fhdr->color_space = static_cast<Vp9ColorSpace>(reader_.ReadLiteral(3));
  if (fhdr->color_space != Vp9ColorSpace::SRGB) {
    fhdr->color_range = reader_.ReadBool();
    if (fhdr->profile == 1 || fhdr->profile == 3) {
      fhdr->subsampling_x = reader_.ReadBool() ? 1 : 0;
      fhdr->subsampling_y = reader_.ReadBool() ? 1 : 0;
      if (fhdr->subsampling_x == 1 && fhdr->subsampling_y == 1) {
        DVLOG(1) << "4:2:0 color not supported in profile 1 or 3";
        return false;
      }
      bool reserved = reader_.ReadBool();
      if (reserved) {
        DVLOG(1) << "reserved bit set";
        return false;
      }
    } else {
      fhdr->subsampling_x = fhdr->subsampling_y = 1;
    }
  } else {
    fhdr->color_range = true;
    if (fhdr->profile == 1 || fhdr->profile == 3) {
      fhdr->subsampling_x = fhdr->subsampling_y = 0;

      bool reserved = reader_.ReadBool();
      if (reserved) {
        DVLOG(1) << "reserved bit set";
        return false;
      }
    } else {
      DVLOG(1) << "4:4:4 color not supported in profile 0 or 2";
      return false;
    }
  }

  return true;
}

// 6.2.3 Frame size syntax
void Vp9UncompressedHeaderParser::ReadFrameSize(Vp9FrameHeader* fhdr) {
  fhdr->frame_width = reader_.ReadLiteral(16) + 1;
  fhdr->frame_height = reader_.ReadLiteral(16) + 1;
}

// 6.2.4 Render size syntax
void Vp9UncompressedHeaderParser::ReadRenderSize(Vp9FrameHeader* fhdr) {
  if (reader_.ReadBool()) {
    fhdr->render_width = reader_.ReadLiteral(16) + 1;
    fhdr->render_height = reader_.ReadLiteral(16) + 1;
  } else {
    fhdr->render_width = fhdr->frame_width;
    fhdr->render_height = fhdr->frame_height;
  }
}

// 6.2.5 Frame size with refs syntax
bool Vp9UncompressedHeaderParser::ReadFrameSizeFromRefs(Vp9FrameHeader* fhdr) {
  bool found_ref = false;
  for (const auto& idx : fhdr->ref_frame_idx) {
    found_ref = reader_.ReadBool();
    if (found_ref) {
      const Vp9Parser::ReferenceSlot& ref = context_->GetRefSlot(idx);
      DCHECK(ref.initialized);
      fhdr->frame_width = ref.frame_width;
      fhdr->frame_height = ref.frame_height;

      const unsigned kMaxDimension = 1u << 16;
      DCHECK_LE(fhdr->frame_width, kMaxDimension);
      DCHECK_LE(fhdr->frame_height, kMaxDimension);
      break;
    }
  }

  if (!found_ref)
    ReadFrameSize(fhdr);

  // 7.2.5 Frame size with refs semantics
  bool has_valid_ref_frame = false;
  for (const auto& idx : fhdr->ref_frame_idx) {
    const Vp9Parser::ReferenceSlot& ref = context_->GetRefSlot(idx);
    if (2 * fhdr->frame_width >= ref.frame_width &&
        2 * fhdr->frame_height >= ref.frame_height &&
        fhdr->frame_width <= 16 * ref.frame_width &&
        fhdr->frame_height <= 16 * ref.frame_height) {
      has_valid_ref_frame = true;
      break;
    }
  }
  if (!has_valid_ref_frame) {
    DVLOG(1) << "There should be at least one reference frame meeting "
             << "size conditions.";
    return false;
  }

  ReadRenderSize(fhdr);
  return true;
}

// 6.2.7 Interpolation filter syntax
Vp9InterpolationFilter Vp9UncompressedHeaderParser::ReadInterpolationFilter() {
  if (reader_.ReadBool())
    return Vp9InterpolationFilter::SWITCHABLE;

  // The mapping table for next two bits.
  const Vp9InterpolationFilter table[] = {
      Vp9InterpolationFilter::EIGHTTAP_SMOOTH, Vp9InterpolationFilter::EIGHTTAP,
      Vp9InterpolationFilter::EIGHTTAP_SHARP, Vp9InterpolationFilter::BILINEAR,
  };
  return table[reader_.ReadLiteral(2)];
}

void Vp9UncompressedHeaderParser::SetupPastIndependence(Vp9FrameHeader* fhdr) {
  memset(&context_->segmentation_, 0, sizeof(context_->segmentation_));
  memset(fhdr->ref_frame_sign_bias, 0, sizeof(fhdr->ref_frame_sign_bias));

  ResetLoopfilter();
  fhdr->frame_context = kVp9DefaultFrameContext;
  DCHECK(fhdr->frame_context.IsValid());
}

// 6.2.8 Loop filter params syntax
void Vp9UncompressedHeaderParser::ReadLoopFilterParams() {
  Vp9LoopFilterParams& loop_filter = context_->loop_filter_;

  loop_filter.level = reader_.ReadLiteral(6);
  loop_filter.sharpness = reader_.ReadLiteral(3);
  loop_filter.delta_update = false;

  loop_filter.delta_enabled = reader_.ReadBool();
  if (loop_filter.delta_enabled) {
    loop_filter.delta_update = reader_.ReadBool();
    if (loop_filter.delta_update) {
      for (size_t i = 0; i < Vp9RefType::VP9_FRAME_MAX; i++) {
        loop_filter.update_ref_deltas[i] = reader_.ReadBool();
        if (loop_filter.update_ref_deltas[i])
          loop_filter.ref_deltas[i] = reader_.ReadSignedLiteral(6);
      }

      for (size_t i = 0; i < Vp9LoopFilterParams::kNumModeDeltas; i++) {
        loop_filter.update_mode_deltas[i] = reader_.ReadBool();
        if (loop_filter.update_mode_deltas[i])
          loop_filter.mode_deltas[i] = reader_.ReadSignedLiteral(6);
      }
    }
  }
}

// 6.2.9 Quantization params syntax
void Vp9UncompressedHeaderParser::ReadQuantizationParams(
    Vp9QuantizationParams* quants) {
  quants->base_q_idx = reader_.ReadLiteral(8);

  quants->delta_q_y_dc = ReadDeltaQ();
  quants->delta_q_uv_dc = ReadDeltaQ();
  quants->delta_q_uv_ac = ReadDeltaQ();
}

// 6.2.10 Delta quantizer syntax
int8_t Vp9UncompressedHeaderParser::ReadDeltaQ() {
  if (reader_.ReadBool())
    return reader_.ReadSignedLiteral(4);
  return 0;
}

// 6.2.11 Segmentation params syntax
bool Vp9UncompressedHeaderParser::ReadSegmentationParams() {
  Vp9SegmentationParams& segmentation = context_->segmentation_;
  segmentation.update_map = false;
  segmentation.update_data = false;

  segmentation.enabled = reader_.ReadBool();
  if (!segmentation.enabled)
    return true;

  segmentation.update_map = reader_.ReadBool();
  if (segmentation.update_map) {
    for (auto& tree_prob : segmentation.tree_probs) {
      tree_prob = ReadProb();
    }

    segmentation.temporal_update = reader_.ReadBool();
    for (auto& pred_prob : segmentation.pred_probs) {
      pred_prob = segmentation.temporal_update ? ReadProb() : kVp9MaxProb;
    }
  }

  segmentation.update_data = reader_.ReadBool();
  if (segmentation.update_data) {
    segmentation.abs_or_delta_update = reader_.ReadBool();

    const int kFeatureDataBits[] = {8, 6, 2, 0};
    const bool kFeatureDataSigned[] = {true, true, false, false};

    for (size_t i = 0; i < Vp9SegmentationParams::kNumSegments; i++) {
      for (size_t j = 0; j < Vp9SegmentationParams::SEG_LVL_MAX; j++) {
        int16_t data = 0;
        segmentation.feature_enabled[i][j] = reader_.ReadBool();
        if (segmentation.feature_enabled[i][j]) {
          data = reader_.ReadLiteral(kFeatureDataBits[j]);
          if (kFeatureDataSigned[j])
            if (reader_.ReadBool()) {
              // 7.2.9
              if (segmentation.abs_or_delta_update) {
                DVLOG(1) << "feature_sign should be 0"
                         << " if abs_or_delta_update is 1";
                return false;
              }
              data = -data;
            }
        }
        segmentation.feature_data[i][j] = data;
      }
    }
  }
  return true;
}

// 6.2.12 Probability syntax
uint8_t Vp9UncompressedHeaderParser::ReadProb() {
  return reader_.ReadBool() ? reader_.ReadLiteral(8) : kVp9MaxProb;
}

// 6.2.13 Tile info syntax
bool Vp9UncompressedHeaderParser::ReadTileInfo(Vp9FrameHeader* fhdr) {
  int sb64_cols = (fhdr->frame_width + 63) / 64;

  int min_log2_tile_cols = GetMinLog2TileCols(sb64_cols);
  int max_log2_tile_cols = GetMaxLog2TileCols(sb64_cols);

  int max_ones = max_log2_tile_cols - min_log2_tile_cols;
  fhdr->tile_cols_log2 = min_log2_tile_cols;
  while (max_ones-- && reader_.ReadBool())
    fhdr->tile_cols_log2++;

  fhdr->tile_rows_log2 = reader_.ReadBool() ? 1 : 0;
  if (fhdr->tile_rows_log2 > 0 && reader_.ReadBool())
    fhdr->tile_rows_log2++;

  // 7.2.11 Tile info semantics
  if (fhdr->tile_cols_log2 > 6) {
    DVLOG(1) << "tile_cols_log2 should be <= 6";
    return false;
  }

  return true;
}

void Vp9UncompressedHeaderParser::ResetLoopfilter() {
  Vp9LoopFilterParams& loop_filter = context_->loop_filter_;

  loop_filter.delta_enabled = true;
  loop_filter.delta_update = true;

  loop_filter.ref_deltas[VP9_FRAME_INTRA] = 1;
  loop_filter.ref_deltas[VP9_FRAME_LAST] = 0;
  loop_filter.ref_deltas[VP9_FRAME_GOLDEN] = -1;
  loop_filter.ref_deltas[VP9_FRAME_ALTREF] = -1;

  memset(loop_filter.mode_deltas, 0, sizeof(loop_filter.mode_deltas));
}

// 6.2 Uncompressed header syntax
bool Vp9UncompressedHeaderParser::Parse(const uint8_t* stream,
                                        off_t frame_size,
                                        Vp9FrameHeader* fhdr) {
  DVLOG(2) << "Vp9UncompressedHeaderParser::Parse";
  reader_.Initialize(stream, frame_size);

  fhdr->data = stream;
  fhdr->frame_size = frame_size;

  // frame marker
  if (reader_.ReadLiteral(2) != 0x2) {
    DVLOG(1) << "frame marker shall be equal to 2";
    return false;
  }

  fhdr->profile = ReadProfile();
  if (fhdr->profile >= kVp9MaxProfile) {
    DVLOG(1) << "Unsupported bitstream profile";
    return false;
  }

  fhdr->show_existing_frame = reader_.ReadBool();
  if (fhdr->show_existing_frame) {
    fhdr->frame_to_show_map_idx = reader_.ReadLiteral(3);
    fhdr->show_frame = true;

    if (!reader_.ConsumeTrailingBits()) {
      DVLOG(1) << "trailing bits are not zero";
      return false;
    }
    if (!reader_.IsValid()) {
      DVLOG(1) << "parser reads beyond the end of buffer";
      return false;
    }
    fhdr->uncompressed_header_size = reader_.GetBytesRead();
    fhdr->header_size_in_bytes = 0;
    return true;
  }

  fhdr->frame_type = static_cast<Vp9FrameHeader::FrameType>(reader_.ReadBool());
  fhdr->show_frame = reader_.ReadBool();
  fhdr->error_resilient_mode = reader_.ReadBool();

  if (fhdr->IsKeyframe()) {
    if (!VerifySyncCode())
      return false;

    if (!ReadColorConfig(fhdr))
      return false;

    ReadFrameSize(fhdr);
    ReadRenderSize(fhdr);
    fhdr->refresh_frame_flags = 0xff;
  } else {
    if (!fhdr->show_frame)
      fhdr->intra_only = reader_.ReadBool();

    if (!fhdr->error_resilient_mode)
      fhdr->reset_frame_context = reader_.ReadLiteral(2);

    if (fhdr->intra_only) {
      if (!VerifySyncCode())
        return false;

      if (fhdr->profile > 0) {
        if (!ReadColorConfig(fhdr))
          return false;
      } else {
        fhdr->bit_depth = 8;
        fhdr->color_space = Vp9ColorSpace::BT_601;
        fhdr->subsampling_x = fhdr->subsampling_y = 1;
      }

      fhdr->refresh_frame_flags = reader_.ReadLiteral(8);

      ReadFrameSize(fhdr);
      ReadRenderSize(fhdr);
    } else {
      fhdr->refresh_frame_flags = reader_.ReadLiteral(8);

      static_assert(std::extent<decltype(fhdr->ref_frame_sign_bias)>() >=
                        Vp9RefType::VP9_FRAME_LAST + kVp9NumRefsPerFrame,
                    "ref_frame_sign_bias is not big enough");
      for (size_t i = 0; i < kVp9NumRefsPerFrame; i++) {
        fhdr->ref_frame_idx[i] = reader_.ReadLiteral(kVp9NumRefFramesLog2);
        fhdr->ref_frame_sign_bias[Vp9RefType::VP9_FRAME_LAST + i] =
            reader_.ReadBool();

        // 8.2 Frame order constraints
        // ref_frame_idx[i] refers to an earlier decoded frame.
        const Vp9Parser::ReferenceSlot& ref =
            context_->GetRefSlot(fhdr->ref_frame_idx[i]);
        if (!ref.initialized) {
          DVLOG(1) << "ref_frame_idx[" << i
                   << "]=" << static_cast<int>(fhdr->ref_frame_idx[i])
                   << " refers to unused frame";
          return false;
        }

        // 7.2 Uncompressed header semantics
        // the selected reference frames match the current frame in bit depth,
        // profile, chroma subsampling, and color space.
        if (ref.profile != fhdr->profile) {
          DVLOG(1) << "profile of referenced frame mismatch";
          return false;
        }
        if (i == 0) {
          // Below fields are not specified for inter-frame in header, so copy
          // them from referenced frame.
          fhdr->bit_depth = ref.bit_depth;
          fhdr->color_space = ref.color_space;
          fhdr->subsampling_x = ref.subsampling_x;
          fhdr->subsampling_y = ref.subsampling_y;
        } else {
          if (fhdr->bit_depth != ref.bit_depth) {
            DVLOG(1) << "bit_depth of referenced frame mismatch";
            return false;
          }
          // There are encoded streams with no color_space information
          // with in the frame header of an Intra only frame, so we assigned the
          // default color_space :BT_601 as per the spec. But reference list
          // might have frames with UNKNOWN color space information too. So we
          // relax the requirement a bit to cover more video samples and added
          // an exception for UNKNOWN colorspace
          if (fhdr->color_space != ref.color_space &&
              fhdr->color_space != Vp9ColorSpace::UNKNOWN &&
              ref.color_space != Vp9ColorSpace::UNKNOWN) {
            DVLOG(1) << "color_space of referenced frame mismatch";
            return false;
          }
          if (fhdr->subsampling_x != ref.subsampling_x ||
              fhdr->subsampling_y != ref.subsampling_y) {
            DVLOG(1) << "chroma subsampling of referenced frame mismatch";
            return false;
          }
        }
      }

      if (!ReadFrameSizeFromRefs(fhdr))
        return false;

      fhdr->allow_high_precision_mv = reader_.ReadBool();
      fhdr->interpolation_filter = ReadInterpolationFilter();
    }
  }

  if (fhdr->error_resilient_mode) {
    fhdr->refresh_frame_context = false;
    fhdr->frame_parallel_decoding_mode = true;
  } else {
    fhdr->refresh_frame_context = reader_.ReadBool();
    fhdr->frame_parallel_decoding_mode = reader_.ReadBool();
  }

  fhdr->frame_context_idx_to_save_probs = fhdr->frame_context_idx =
      reader_.ReadLiteral(kVp9NumFrameContextsLog2);

  if (fhdr->IsIntra() || fhdr->error_resilient_mode) {
    SetupPastIndependence(fhdr);
    fhdr->frame_context_idx = 0;
  }

  ReadLoopFilterParams();
  // Update loop_filter in current_frame_hdr
  fhdr->loop_filter = context_->loop_filter_;
  ReadQuantizationParams(&fhdr->quant_params);
  if (!ReadSegmentationParams())
    return false;
  // Update segmentation in current_frame_hdr
  fhdr->segmentation = context_->segmentation_;
  if (!ReadTileInfo(fhdr))
    return false;

  fhdr->header_size_in_bytes = reader_.ReadLiteral(16);
  if (fhdr->header_size_in_bytes == 0) {
    DVLOG(1) << "invalid header size";
    return false;
  }

  if (!reader_.ConsumeTrailingBits()) {
    DVLOG(1) << "trailing bits are not zero";
    return false;
  }
  if (!reader_.IsValid()) {
    DVLOG(1) << "parser reads beyond the end of buffer";
    return false;
  }
  fhdr->uncompressed_header_size = reader_.GetBytesRead();

  return true;
}

}  // namespace media
