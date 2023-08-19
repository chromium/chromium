// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if BUILDFLAG(IS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY) && \
    (defined(COMPILER_GCC) || defined(__clang__))

#include "media/gpu/v4l2/mt21/mt21_decompressor.h"

#include <stdlib.h>
#include <unistd.h>
#include <algorithm>

#include "base/bits.h"
#include "base/command_line.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"
#include "third_party/libyuv/include/libyuv/planar_functions.h"

namespace media {
namespace {

constexpr size_t kBitsInByte = 8;
constexpr size_t kMT21SubblockWidth = 16;
constexpr size_t kMT21SubblockHeight = 4;
constexpr size_t kMT21SubblockSize = kMT21SubblockWidth * kMT21SubblockHeight;
constexpr size_t kMT21BlockSize = 2 * kMT21SubblockSize;
constexpr size_t kMT21TileHeight = 32;
constexpr size_t kMT21YFooterAlignment = 4096;
constexpr size_t kMT21UVFooterAlignment = kMT21YFooterAlignment / 2;

// Utility class to write a compressed MT21 block. We use this for generating
// synthetic compressed frames.
class MT21BlockWriter {
 public:
  MT21BlockWriter(uint8_t* block);

  bool WriteBit(bool bit);
  bool WriteNBits(int val, int n);
  void PadToRow();
  size_t GetNumRows();
  void SetPos(size_t bit_idx);
  size_t GetPos();

 private:
  uint8_t* block_;
  size_t bit_idx_;
};

MT21BlockWriter::MT21BlockWriter(uint8_t* block) {
  block_ = block;
  bit_idx_ = 0;
}

bool MT21BlockWriter::WriteBit(bool bit) {
  size_t byte_idx = bit_idx_ / kBitsInByte;
  if (byte_idx >= kMT21BlockSize) {
    return false;
  }

  size_t row = byte_idx / kMT21SubblockWidth;
  size_t col = (kMT21SubblockWidth - 1) - (byte_idx % kMT21SubblockWidth);
  byte_idx = row * kMT21SubblockWidth + col;
  block_[byte_idx] |= (int)bit
                      << ((kBitsInByte - 1) - (bit_idx_ % kBitsInByte));
  bit_idx_++;

  return true;
}

bool MT21BlockWriter::WriteNBits(int val, int n) {
  for (int i = n - 1; i >= 0; i--) {
    if (!WriteBit((val >> i) & 0x1)) {
      return false;
    }
  }

  return true;
}

void MT21BlockWriter::PadToRow() {
  bit_idx_ = base::bits::AlignUp(bit_idx_, kMT21SubblockWidth * kBitsInByte);
}

size_t MT21BlockWriter::GetNumRows() {
  return base::bits::AlignUp(bit_idx_, kMT21SubblockWidth * kBitsInByte) /
         (kMT21SubblockWidth * kBitsInByte);
}

void MT21BlockWriter::SetPos(size_t bit_idx) {
  bit_idx_ = bit_idx;
}

size_t MT21BlockWriter::GetPos() {
  return bit_idx_;
}

// Get a random number according to a double sided geometric distribution. This
// means that outputs further away from zero will be exponentially less likely.
//
// The algorithm we use for generating numbers according to this distribution is
// to first generate a uniform random number, and then count the number of
// leading zeros. In a uniform distribution, every bit has an equal probability
// of being either 0 or 1. This means that the probability of the first N bits
// being 0 is 1/(2^N).
int GeometricRandomNum() {
  uint32_t uniform_random_num = rand();
  int ret = __builtin_clz(uniform_random_num);
  // The above algorithm only generates positive numbers according to a
  // geometric distribution, but we really want both positive and negative.
  if (rand() & 1) {
    return -1 * ret;
  } else {
    return ret;
  }
}

// Encode a value using MT21's Golomb-Rice variant.
void GolombRiceEncode(MT21BlockWriter& writer, int symbol, int k) {
  if (!symbol) {
    writer.WriteNBits(0, k);
    return;
  }

  int base = 1 << k;
  if (symbol < 0) {
    symbol = symbol * -2 + 1;
  } else {
    symbol *= 2;
  }

  int escape_sequence = 7 + k;
  if (symbol / base >= escape_sequence) {
    writer.WriteNBits((1 << escape_sequence) - 1, escape_sequence);
    writer.WriteNBits(symbol - (escape_sequence * base), k >= 4 ? 7 : 8);
    return;
  }

  while (symbol >= base) {
    writer.WriteBit(1);
    symbol -= base;
  }
  writer.WriteBit(0);
  writer.WriteNBits(symbol, k);
}

// Unoptimized version of our pixel prediction algorithm. This is essentially a
// copy of the version here:
// https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/drm-tests/pixel_formats/mt21_converter.c;drc=091692f34d333dec8fd3a8e375a4ad5a65682cb2;l=173
uint8_t PredictPixelValue(const uint8_t* subblock, int x, int y, int width) {
  if (y == 0) {
    return subblock[x + 1];
  } else if (x == width - 1) {
    return subblock[(y - 1) * width + x];
  } else if (x == 0) {
    int up_right = subblock[(y - 1) * width + x + 1];
    int up = subblock[(y - 1) * width + x];
    int right = subblock[y * width + x + 1];
    int max_up_right = std::max(up, right);
    int min_up_right = std::min(up, right);

    if (up_right <= max_up_right && up_right >= min_up_right) {
      return right + up - up_right;
    } else if (up_right > max_up_right) {
      return max_up_right;
    } else {
      return min_up_right;
    }
  } else {
    int up_left = subblock[(y - 1) * width + x - 1];
    int up_right = subblock[(y - 1) * width + x + 1];
    int up = subblock[(y - 1) * width + x];
    int right = subblock[y * width + x + 1];
    int max_up_right = std::max(up, right);
    int min_up_right = std::min(up, right);

    if (up_right <= max_up_right && up_right >= min_up_right) {
      return right + up - up_right;
    } else if (up_left <= max_up_right && up_left >= min_up_right) {
      return up - up_left + right;
    } else if (up_left >= max_up_right) {
      return max_up_right;
    } else {
      return min_up_right;
    }
  }
}

void GenerateRandomSubblock(MT21BlockWriter& writer,
                            uint8_t* golden_subblock,
                            int width) {
  constexpr int k = 1;
  uint8_t top_right = (rand() & 0x7F) + 0x7F;

  golden_subblock[width - 1] = top_right;

  writer.WriteNBits(k - 1, 3);
  writer.WriteNBits(top_right, 8);

  for (size_t y = 0; y < kMT21SubblockHeight; y++) {
    for (int x = (y ? (width - 1) : (width - 2)); x >= 0; x--) {
      int random_delta = GeometricRandomNum();

      GolombRiceEncode(writer, random_delta, k);

      golden_subblock[y * width + x] =
          PredictPixelValue(golden_subblock, x, y, width) + random_delta;
    }
  }
}

void GenerateRandomYBlock(uint8_t* mt21_block,
                          uint8_t* mm21_block,
                          size_t& subblock_len1,
                          size_t& subblock_len2) {
  MT21BlockWriter writer(mt21_block);

  // There's a chance we generate a subblock that is too entropic to compress,
  // so we just re-do until we get a good subblock.
  do {
    writer.SetPos(0);
    GenerateRandomSubblock(writer, mm21_block, kMT21SubblockWidth);
  } while (writer.GetNumRows() >= kMT21SubblockHeight);

  subblock_len1 = writer.GetNumRows();
  writer.PadToRow();

  int bit_idx = writer.GetPos();
  do {
    writer.SetPos(bit_idx);
    GenerateRandomSubblock(writer, mm21_block + kMT21SubblockSize,
                           kMT21SubblockWidth);
  } while (writer.GetNumRows() - subblock_len1 >= kMT21SubblockHeight);
  subblock_len2 = writer.GetNumRows() - subblock_len1;
}

void InterleaveUV(uint8_t* mm21_subblock,
                  uint8_t* u_subblock,
                  uint8_t* v_subblock) {
  for (size_t i = 0; i < kMT21SubblockWidth * kMT21SubblockHeight / 2; i++) {
    *mm21_subblock = *u_subblock;
    mm21_subblock++;
    u_subblock++;
    *mm21_subblock = *v_subblock;
    mm21_subblock++;
    v_subblock++;
  }
}

void GenerateRandomUVBlock(uint8_t* mt21_block,
                           uint8_t* mm21_block,
                           size_t& subblock_len1,
                           size_t& subblock_len2) {
  MT21BlockWriter writer(mt21_block);
  uint8_t v_subblock[kMT21SubblockWidth / 2 * kMT21SubblockHeight];
  uint8_t u_subblock[kMT21SubblockWidth / 2 * kMT21SubblockHeight];

  do {
    writer.SetPos(0);
    GenerateRandomSubblock(writer, v_subblock, kMT21SubblockWidth / 2);
    GenerateRandomSubblock(writer, u_subblock, kMT21SubblockWidth / 2);
  } while (writer.GetNumRows() >= kMT21SubblockHeight);

  writer.PadToRow();
  subblock_len1 = writer.GetNumRows();
  InterleaveUV(mm21_block, u_subblock, v_subblock);

  int bit_idx = writer.GetPos();
  do {
    writer.SetPos(bit_idx);
    GenerateRandomSubblock(writer, v_subblock, kMT21SubblockWidth / 2);
    GenerateRandomSubblock(writer, u_subblock, kMT21SubblockWidth / 2);
  } while (writer.GetNumRows() - subblock_len1 >= kMT21SubblockHeight);

  subblock_len2 = writer.GetNumRows() - subblock_len1;
  InterleaveUV(mm21_block + kMT21SubblockSize, u_subblock, v_subblock);
}

void GenerateRandomCompressedFrame(uint8_t* mt21_frame_y,
                                   uint8_t* mt21_frame_uv,
                                   uint8_t* mt21_footer_y,
                                   uint8_t* mt21_footer_uv,
                                   uint8_t* nv12_frame_y,
                                   uint8_t* nv12_frame_uv,
                                   int width,
                                   int height) {
  uint8_t* mm21_frame_y =
      static_cast<uint8_t*>(aligned_alloc(16, width * height));
  uint8_t* mm21_frame_uv =
      static_cast<uint8_t*>(aligned_alloc(16, width * height / 2));

  uint8_t* mm21_block = mm21_frame_y;
  size_t subblock_len1;
  size_t subblock_len2;
  for (int i = 0; i < width * height; i += kMT21BlockSize) {
    GenerateRandomYBlock(mt21_frame_y, mm21_block, subblock_len1,
                         subblock_len2);
    mt21_frame_y += kMT21BlockSize;
    mm21_block += kMT21BlockSize;
    subblock_len1--;
    subblock_len2--;
    if ((i / kMT21BlockSize) % 2 == 0) {
      mt21_footer_y[i / kMT21BlockSize / 2] |=
          subblock_len1 | (subblock_len2 << 2);
    } else {
      mt21_footer_y[i / kMT21BlockSize / 2] |=
          (subblock_len1 << 4) | (subblock_len2 << 6);
    }
  }
  mm21_block = mm21_frame_uv;
  for (int i = 0; i < width * height / 2; i += kMT21BlockSize) {
    GenerateRandomUVBlock(mt21_frame_uv, mm21_block, subblock_len1,
                          subblock_len2);
    mt21_frame_uv += kMT21BlockSize;
    mm21_block += kMT21BlockSize;
    subblock_len1--;
    subblock_len2--;
    if ((i / kMT21BlockSize) % 2 == 0) {
      mt21_footer_uv[i / kMT21BlockSize / 2] |=
          subblock_len1 | (subblock_len2 << 2);
    } else {
      mt21_footer_uv[i / kMT21BlockSize / 2] |=
          (subblock_len1 << 4) | (subblock_len2 << 6);
    }
  }

  libyuv::DetilePlane(mm21_frame_y, width, nv12_frame_y, width, width, height,
                      kMT21TileHeight);
  libyuv::DetilePlane(mm21_frame_uv, width, nv12_frame_uv, width, width,
                      height / 2, kMT21TileHeight / 2);

  free(mm21_frame_y);
  free(mm21_frame_uv);
}

void AllocateMT21Plane(gfx::Size& resolution,
                       bool is_chroma,
                       size_t& plane_size,
                       uint8_t** plane,
                       size_t& footer_offset) {
  plane_size = resolution.GetArea();
  if (is_chroma) {
    plane_size /= 2;
    footer_offset = base::bits::AlignUp(
        plane_size, static_cast<size_t>(kMT21UVFooterAlignment));
  } else {
    footer_offset = base::bits::AlignUp(
        plane_size, static_cast<size_t>(kMT21YFooterAlignment));
  }
  size_t footer_size = base::bits::AlignUp(plane_size / kMT21SubblockSize * 2,
                                           static_cast<size_t>(kBitsInByte)) /
                       kBitsInByte;
  plane_size = footer_offset + footer_size;
  *plane = static_cast<uint8_t*>(aligned_alloc(16, plane_size));
}

TEST(MT21DecompressorTest, TestMT21DecompressorPerfTest) {
  gfx::Size resolution(1920, 1088);
  uint8_t* golden_y =
      static_cast<uint8_t*>(aligned_alloc(16, resolution.GetArea()));
  uint8_t* decompressed_y =
      static_cast<uint8_t*>(aligned_alloc(16, resolution.GetArea()));
  uint8_t* golden_uv =
      static_cast<uint8_t*>(aligned_alloc(16, resolution.GetArea() / 2));
  uint8_t* decompressed_uv =
      static_cast<uint8_t*>(aligned_alloc(16, resolution.GetArea() / 2));
  uint8_t* mt21_y;
  uint8_t* mt21_uv;
  size_t mt21_y_size, mt21_uv_size, mt21_y_footer_offset, mt21_uv_footer_offset;

  AllocateMT21Plane(resolution, false, mt21_y_size, &mt21_y,
                    mt21_y_footer_offset);
  AllocateMT21Plane(resolution, true, mt21_uv_size, &mt21_uv,
                    mt21_uv_footer_offset);

  GenerateRandomCompressedFrame(mt21_y, mt21_uv, mt21_y + mt21_y_footer_offset,
                                mt21_uv + mt21_uv_footer_offset, golden_y,
                                golden_uv, resolution.width(),
                                resolution.height());

  MT21Decompressor decompressor(resolution);

  perf_test::PerfResultReporter reporter("MT21Decompressor", "Uncapped Test");
  reporter.RegisterImportantMetric(".decompress_latency", "us");

  memset(decompressed_y, 0, resolution.GetArea());
  memset(decompressed_uv, 0, resolution.GetArea() / 2);
  constexpr int kNumIterations = 1000;
  auto start_time = base::TimeTicks::Now();
  for (int i = 0; i < kNumIterations; i++) {
    decompressor.MT21ToNV12(mt21_y, mt21_uv, mt21_y_size, mt21_uv_size,
                            decompressed_y, decompressed_uv);
  }
  auto end_time = base::TimeTicks::Now();
  auto delta_time = end_time - start_time;
  reporter.AddResult(
      ".decompress_latency",
      static_cast<size_t>(delta_time.InMicroseconds() / kNumIterations));

  for (int i = 0; i < resolution.GetArea(); i++) {
    ASSERT_TRUE(decompressed_y[i] == golden_y[i]);
  }
  for (int i = 0; i < resolution.GetArea() / 2; i++) {
    ASSERT_TRUE(decompressed_uv[i] == golden_uv[i]);
  }

  free(golden_y);
  free(decompressed_y);
  free(golden_uv);
  free(decompressed_uv);
  free(mt21_y);
  free(mt21_uv);
}

}  // namespace
}  // namespace media

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);

  testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}

#endif
