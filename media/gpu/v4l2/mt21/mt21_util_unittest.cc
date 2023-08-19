// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if BUILDFLAG(IS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY) && \
    (defined(COMPILER_GCC) || defined(__clang__))

#include "testing/gtest/include/gtest/gtest.h"

#include "media/gpu/v4l2/mt21/mt21_util.h"

#include "base/command_line.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace media {
namespace {

TEST(MT21UtilTest, TestBitstreamReader) {
  uint8_t buf[64] = {
      0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
      0x55, 0x55, 0x55, 0x55, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  MT21BitstreamReader reader(buf);

  ASSERT_TRUE(reader.ReadNBits(0) == 0x00);
  ASSERT_TRUE(reader.PeekNBits(8) == 0x55);
  reader.DiscardNBits(1);
  ASSERT_TRUE(reader.ReadNBits(7) == 0x55);
  reader.DiscardNBits(24);
  reader.DiscardNBits(32);
  reader.DiscardNBits(32);
  reader.DiscardNBits(32);
  ASSERT_TRUE(reader.ReadNBits(8) == 0xFF);
}

TEST(MT21UtilTest, TestReadGolombRiceSymbol) {
  constexpr int k = 2;
  uint8_t buf[64] = {0};
  // 00=0  010=1  011=-1 1000=2  11111111100000001=-18
  buf[15] = 0b00010011;
  buf[14] = 0b10001111;
  buf[13] = 0b11111000;
  buf[12] = 0b00001000;
  MT21BitstreamReader reader(buf);

  ASSERT_TRUE(ReadGolombRiceSymbol(reader, k) == 0);
  ASSERT_TRUE(ReadGolombRiceSymbol(reader, k) == 1);
  ASSERT_TRUE(ReadGolombRiceSymbol(reader, k) == -1);
  ASSERT_TRUE(ReadGolombRiceSymbol(reader, k) == 2);
  ASSERT_TRUE(ReadGolombRiceSymbol(reader, k) == -18);
}

TEST(MT21UtilTest, TestFastReadGolombRiceSymbol) {
  GolombRiceTableEntry cache[kGolombRiceCacheSize];
  PopulateGolombRiceCache(cache);
  constexpr int k = 2;
  uint8_t buf[64] = {0};
  // 00=0  010=1 011=-1 1000 = 2 11111111100000001=-18
  buf[15] = 0b00010011;
  buf[14] = 0b10001111;
  buf[13] = 0b11111000;
  buf[12] = 0b00001000;
  MT21BitstreamReader reader(buf);

  ASSERT_TRUE(cache[0].in_size);
  ASSERT_TRUE(FastReadGolombRiceSymbol(
                  reader, k, cache + (k - 1) * kGolombRiceTableSize) == 0);
  ASSERT_TRUE(FastReadGolombRiceSymbol(
                  reader, k, cache + (k - 1) * kGolombRiceTableSize) == 1);
  ASSERT_TRUE(FastReadGolombRiceSymbol(
                  reader, k, cache + (k - 1) * kGolombRiceTableSize) == -1);
  ASSERT_TRUE(FastReadGolombRiceSymbol(
                  reader, k, cache + (k - 1) * kGolombRiceTableSize) == 2);
  ASSERT_TRUE(FastReadGolombRiceSymbol(
                  reader, k, cache + (k - 1) * kGolombRiceTableSize) == -18);
}

TEST(MT21UtilTest, TestPredictionMethods) {
  ASSERT_TRUE(FirstRowPrediction(0x80) == 0x80);

  ASSERT_TRUE(LastColPrediction(0x80) == 0x80);

  ASSERT_TRUE(FirstColPrediction(0x80, 0x7E, 0x70) == 0x72);
  ASSERT_TRUE(FirstColPrediction(0x80, 0x83, 0x70) == 0x80);
  ASSERT_TRUE(FirstColPrediction(0x80, 0x6F, 0x70) == 0x70);

  ASSERT_TRUE(BodyPrediction(0x81, 0x80, 0x7E, 0x70) == 0x72);
  ASSERT_TRUE(BodyPrediction(0x7F, 0x80, 0x83, 0x70) == 0x71);
  ASSERT_TRUE(BodyPrediction(0x82, 0x80, 0x83, 0x70) == 0x80);
  ASSERT_TRUE(BodyPrediction(0x6F, 0x80, 0x83, 0x70) == 0x70);
}

TEST(MT21UtilTest, TestSolidColorBlocks) {
  GolombRiceTableEntry cache[kGolombRiceCacheSize];
  PopulateGolombRiceCache(cache);
  uint8_t buf[64] = {0};
  uint8_t dest_buf[64] = {0};

  buf[15] = 0xF0;
  buf[14] = 0x1D;
  buf[13] = 0xFC;
  MT21YSubblock y_subblock = {buf, dest_buf};
  DecompressSubblockHelper(y_subblock, cache);
  for (size_t i = 0; i < 64; i++) {
    ASSERT_TRUE(dest_buf[i] == 0x80);
  }

  MT21UVSubblock uv_subblock = {buf, dest_buf};
  DecompressSubblockHelper(uv_subblock, cache);
  for (size_t i = 0; i < 64; i++) {
    if (i % 2) {
      ASSERT_TRUE(dest_buf[i] == 0x80);
    } else {
      ASSERT_TRUE(dest_buf[i] == 0x7F);
    }
  }
}

TEST(MT21UtilTest, TestCompressedBlocks) {
  GolombRiceTableEntry cache[kGolombRiceCacheSize];
  PopulateGolombRiceCache(cache);
  uint8_t buf[64] = {
      0x00, 0x00, 0x40, 0x49, 0x01, 0x22, 0x29, 0x02, 0x08, 0x43, 0x41,
      0x20, 0x02, 0x08, 0x48, 0x1A, 0x00, 0x00, 0x00, 0x00, 0x20, 0x08,
      0x20, 0x10, 0x80, 0x10, 0x00, 0x84, 0x45, 0x01, 0x08, 0x1B, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  uint8_t dest_buf[64] = {0};
  uint8_t expected_buf[64] = {
      0xD4, 0xD4, 0xD4, 0xD4, 0xD4, 0xD4, 0xD4, 0xD4, 0xD3, 0xD3, 0xD3,
      0xD3, 0xD3, 0xD3, 0xD2, 0xD2, 0xD4, 0xD4, 0xD4, 0xD4, 0xD4, 0xD5,
      0xD5, 0xD5, 0xD5, 0xD4, 0xD4, 0xD4, 0xD4, 0xD4, 0xD3, 0xD3, 0xD5,
      0xD5, 0xD5, 0xD6, 0xD6, 0xD6, 0xD6, 0xD6, 0xD6, 0xD6, 0xD6, 0xD6,
      0xD6, 0xD5, 0xD5, 0xD5, 0xD6, 0xD6, 0xD7, 0xD7, 0xD7, 0xD7, 0xD8,
      0xD8, 0xD8, 0xD8, 0xD8, 0xD8, 0xD8, 0xD7, 0xD7, 0xD6,
  };

  MT21YSubblock y_subblock = {buf, dest_buf};
  DecompressSubblockHelper(y_subblock, cache);
  for (size_t i = 0; i < 64; i++) {
    ASSERT_TRUE(dest_buf[i] == expected_buf[i]);
  }
}

TEST(MT21UtilTest, TestVectorReadGolombRiceSymbol) {
  uint32x4_t accumulator[4];
  uint32x4_t outstanding_reads[4] = {{0}};
  uint32x4_t escape_codes[4];
  uint32x4_t escape_binary_len_diff[4];
  uint32x4_t k_vals[4];
  uint32x4_t dword_solid_color_mask[4];
  uint8x16_t solid_color_mask;
  uint8_t buf[64] = {0};
  // 001=(k=2)  10000000=(top_right=0x80)
  buf[63] = 0b00110000;
  // 00=0  010=1
  buf[62] = 0b00000010;
  // 011=-1  1000=2  11111111100000001=-18
  buf[61] = 0b01110001;
  buf[60] = 0b11111111;
  buf[59] = 0b00000001;
  uint8_t* compressed_ptr[16] = {
      buf + 60, buf + 60, buf + 60, buf + 60, buf + 60, buf + 60,
      buf + 60, buf + 60, buf + 60, buf + 60, buf + 60, buf + 60,
      buf + 60, buf + 60, buf + 60, buf + 60,
  };

  VectorInitializeAccumulator(accumulator, compressed_ptr);

  uint8x16_t top_right = VectorReadCompressedHeader(
      accumulator, outstanding_reads, escape_codes, escape_binary_len_diff,
      k_vals, dword_solid_color_mask, solid_color_mask, compressed_ptr);
  for (int i = 0; i < 16; i++) {
    ASSERT_TRUE(top_right[i] == 0x80);
    ASSERT_TRUE(k_vals[i / 4][i % 4] == 2);
  }

  uint8x16_t symbol;
  symbol = VectorReadGolombRiceSymbol(
      accumulator, outstanding_reads, escape_codes, escape_binary_len_diff,
      k_vals, dword_solid_color_mask, compressed_ptr);
  for (int i = 0; i < 16; i++) {
    ASSERT_TRUE(vreinterpretq_s8_u8(symbol)[i] == 0);
  }
  symbol = VectorReadGolombRiceSymbol(
      accumulator, outstanding_reads, escape_codes, escape_binary_len_diff,
      k_vals, dword_solid_color_mask, compressed_ptr);
  for (int i = 0; i < 16; i++) {
    ASSERT_TRUE(vreinterpretq_s8_u8(symbol)[i] == 1);
  }
  symbol = VectorReadGolombRiceSymbol(
      accumulator, outstanding_reads, escape_codes, escape_binary_len_diff,
      k_vals, dword_solid_color_mask, compressed_ptr);
  for (int i = 0; i < 16; i++) {
    ASSERT_TRUE(vreinterpretq_s8_u8(symbol)[i] == -1);
  }
  symbol = VectorReadGolombRiceSymbol(
      accumulator, outstanding_reads, escape_codes, escape_binary_len_diff,
      k_vals, dword_solid_color_mask, compressed_ptr);
  for (int i = 0; i < 16; i++) {
    ASSERT_TRUE(vreinterpretq_s8_u8(symbol)[i] == 2);
  }
  symbol = VectorReadGolombRiceSymbol(
      accumulator, outstanding_reads, escape_codes, escape_binary_len_diff,
      k_vals, dword_solid_color_mask, compressed_ptr);
  for (int i = 0; i < 16; i++) {
    ASSERT_TRUE(vreinterpretq_s8_u8(symbol)[i] == -18);
  }
}

TEST(MT21UtilTest, TestVectorPredictionMethods) {
  uint8x16_t pred;

  pred = VectorFirstRowPrediction(vdupq_n_u8(0x80));
  for (int i = 0; i < 16; i++) {
    ASSERT_TRUE(pred[i] == 0x80);
  }

  pred = VectorLastColPrediction(vdupq_n_u8(0x80));
  for (int i = 0; i < 16; i++) {
    ASSERT_TRUE(pred[i] == 0x80);
  }

  pred = VectorFirstColPrediction(vdupq_n_u8(0x80), vdupq_n_u8(0x7E),
                                  vdupq_n_u8(0x70));
  for (int i = 0; i < 16; i++) {
    ASSERT_TRUE(pred[i] == 0x72);
  }
  pred = VectorFirstColPrediction(vdupq_n_u8(0x80), vdupq_n_u8(0x83),
                                  vdupq_n_u8(0x70));
  for (int i = 0; i < 16; i++) {
    ASSERT_TRUE(pred[i] == 0x80);
  }
  pred = VectorFirstColPrediction(vdupq_n_u8(0x80), vdupq_n_u8(0x6F),
                                  vdupq_n_u8(0x70));
  for (int i = 0; i < 16; i++) {
    ASSERT_TRUE(pred[i] == 0x70);
  }

  pred = VectorBodyPrediction(vdupq_n_u8(0x81), vdupq_n_u8(0x80),
                              vdupq_n_u8(0x7E), vdupq_n_u8(0x70));
  for (int i = 0; i < 16; i++) {
    ASSERT_TRUE(pred[i] == 0x72);
  }
  pred = VectorBodyPrediction(vdupq_n_u8(0x7F), vdupq_n_u8(0x80),
                              vdupq_n_u8(0x83), vdupq_n_u8(0x70));
  for (int i = 0; i < 16; i++) {
    ASSERT_TRUE(pred[i] == 0x71);
  }
  pred = VectorBodyPrediction(vdupq_n_u8(0x82), vdupq_n_u8(0x80),
                              vdupq_n_u8(0x83), vdupq_n_u8(0x70));
  for (int i = 0; i < 16; i++) {
    ASSERT_TRUE(pred[i] == 0x80);
  }
  pred = VectorBodyPrediction(vdupq_n_u8(0x6F), vdupq_n_u8(0x80),
                              vdupq_n_u8(0x83), vdupq_n_u8(0x70));
  for (int i = 0; i < 16; i++) {
    ASSERT_TRUE(pred[i] == 0x70);
  }
}

TEST(MT21UtilTest, TestSubblockGather) {
  uint8_t buf[64] = {
      48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
      32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
      16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,
  };
  uint8_t scratch[kMT21ScratchMemorySize] __attribute__((aligned(16)));
  std::vector<MT21Subblock> subblock_list;
  uint8_t* compressed_ptr[16];

  for (int i = 0; i < 16; i++) {
    subblock_list.push_back({buf, nullptr, 64});
  }

  SubblockGather(subblock_list, 0, scratch, compressed_ptr);

  for (int i = 0; i < 16; i++) {
    for (int j = 0; j < 64; j++) {
      ASSERT_TRUE(scratch[1024 + 64 * i + j] == j);
    }
  }
}

TEST(MT21UtilTest, TestSubblockTransposeScatter) {
  uint8_t buf[256] __attribute__((aligned(16)));
  uint8_t* src = buf;
  uint8_t dest_buf[256];
  uint8_t* decompressed_ptr[16];

  for (int i = 0; i < 16; i++) {
    decompressed_ptr[i] = dest_buf + 16 * i;
  }

  for (int i = 0; i < 16; i++) {
    for (int j = 0; j < 16; j++) {
      buf[j * 16 + i] = i * 16 + j;
    }
  }

  SubblockTransposeScatter(src, decompressed_ptr);

  for (int i = 0; i < 16; i++) {
    for (int j = 0; j < 16; j++) {
      ASSERT_TRUE(dest_buf[i * 16 + j] == i * 16 + j);
    }
  }
}

TEST(MT21UtilTest, TestVectorSolidColorBlocks) {
  uint8_t buf[64] = {0};
  uint8_t dest_buf[64 * 16] = {0};
  std::vector<MT21YSubblock> y_subblocks;
  std::vector<MT21UVSubblock> uv_subblocks;
  uint8_t scratch[kMT21ScratchMemorySize] __attribute__((aligned(16)));

  buf[15] = 0xF0;
  buf[14] = 0x1D;
  buf[13] = 0xFC;
  for (int i = 0; i < 16; i++) {
    y_subblocks.push_back({buf, dest_buf + 64 * i, 16});
    uv_subblocks.push_back({buf, dest_buf + 64 * i, 16});
  }

  VectorDecompressSubblockHelper<MT21YSubblock>(y_subblocks, 0, scratch);
  for (int i = 0; i < 64 * 16; i++) {
    ASSERT_TRUE(dest_buf[i] == 0x80);
  }

  memset(dest_buf, 0, 64 * 16);
  VectorDecompressSubblockHelper<MT21UVSubblock>(uv_subblocks, 0, scratch);
  for (int i = 0; i < 64 * 16; i++) {
    if (i % 2) {
      ASSERT_TRUE(dest_buf[i] == 0x80);
    } else {
      ASSERT_TRUE(dest_buf[i] == 0x7F);
    }
  }
}

TEST(MT21UtilTest, TestVectorCompressedBlocks) {
  uint8_t buf[64] = {
      0x00, 0x00, 0x40, 0x49, 0x01, 0x22, 0x29, 0x02, 0x08, 0x43, 0x41,
      0x20, 0x02, 0x08, 0x48, 0x1A, 0x00, 0x00, 0x00, 0x00, 0x20, 0x08,
      0x20, 0x10, 0x80, 0x10, 0x00, 0x84, 0x45, 0x01, 0x08, 0x1B, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  uint8_t dest_buf[64 * 16] = {0};
  uint8_t expected_buf[64] = {
      0xD4, 0xD4, 0xD4, 0xD4, 0xD4, 0xD4, 0xD4, 0xD4, 0xD3, 0xD3, 0xD3,
      0xD3, 0xD3, 0xD3, 0xD2, 0xD2, 0xD4, 0xD4, 0xD4, 0xD4, 0xD4, 0xD5,
      0xD5, 0xD5, 0xD5, 0xD4, 0xD4, 0xD4, 0xD4, 0xD4, 0xD3, 0xD3, 0xD5,
      0xD5, 0xD5, 0xD6, 0xD6, 0xD6, 0xD6, 0xD6, 0xD6, 0xD6, 0xD6, 0xD6,
      0xD6, 0xD5, 0xD5, 0xD5, 0xD6, 0xD6, 0xD7, 0xD7, 0xD7, 0xD7, 0xD8,
      0xD8, 0xD8, 0xD8, 0xD8, 0xD8, 0xD8, 0xD7, 0xD7, 0xD6,
  };
  std::vector<MT21YSubblock> y_subblocks;
  uint8_t scratch[kMT21ScratchMemorySize] __attribute__((aligned(16)));

  for (int i = 0; i < 16; i++) {
    y_subblocks.push_back({buf, dest_buf + 64 * i, 16});
  }

  VectorDecompressSubblockHelper<MT21YSubblock>(y_subblocks, 0, scratch);
  for (int i = 0; i < 64 * 16; i++) {
    ASSERT_TRUE(dest_buf[i] == expected_buf[i % 64]);
  }
}

TEST(MT21UtilTest, TestSubblockBinning) {
  uint8_t footer[kMT21ScratchMemorySize]
      __attribute__((aligned(kMT21YFooterAlignment)));
  footer[0] = 0b00000111;
  std::vector<MT21YSubblock> bins[2];

  ASSERT_TRUE(ComputeFooterOffset(512, sizeof(footer), kMT21YFooterAlignment) ==
              0);

  BinSubblocks((const uint8_t*)0xDEADBEEF, footer, (uint8_t*)0xC0FFEE, 0, bins);

  ASSERT_TRUE(bins[0].size() == 1);
  ASSERT_TRUE(bins[1].size() == 1);

  ASSERT_TRUE((uint64_t)bins[0][0].src == 0xDEADBEEF + 64);
  ASSERT_TRUE((uint64_t)bins[0][0].dest == 0xC0FFEE + 64);
  ASSERT_TRUE(bins[0][0].len == 32);

  ASSERT_TRUE((uint64_t)bins[1][0].src == 0xDEADBEEF);
  ASSERT_TRUE((uint64_t)bins[1][0].dest == 0xC0FFEE);
  ASSERT_TRUE(bins[1][0].len == 64);
}

}  // namespace
}  // namespace media

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);

  testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}

#endif
