// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/test_utils.h"

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace media::vaapi_test_utils {
namespace {

constexpr size_t kTile4TileWidthBytes = 128;
constexpr size_t kTile4TileHeightBytes = 32;
constexpr size_t kTile4TileSizeBytes =
    kTile4TileWidthBytes * kTile4TileHeightBytes;
constexpr size_t kTile4SubtileWidthBytes = 16;
constexpr size_t kTile4SubtileHeightBytes = 4;
constexpr size_t kTile4SubtileSizeBytes =
    kTile4SubtileWidthBytes * kTile4SubtileHeightBytes;

// Computes the byte offset in a Tile4 buffer corresponding to linear
// coordinates (x, y) for a plane with aligned stride `width`.
size_t Tile4Offset(size_t x, size_t y, size_t width) {
  const size_t in_subtile_x = x % kTile4SubtileWidthBytes;
  const size_t in_subtile_y = y % kTile4SubtileHeightBytes;
  const size_t offset_in_subtile =
      in_subtile_y * kTile4SubtileWidthBytes + in_subtile_x;

  const size_t subtile_x = (x / kTile4SubtileWidthBytes) % 8;
  const size_t subtile_y = (y / kTile4SubtileHeightBytes) % 8;
  const size_t z_x = subtile_x / 4;
  const size_t z_y = subtile_y / 2;
  const size_t z_idx = z_y * 2 + z_x;
  const size_t in_z_idx = (subtile_y % 2) * 4 + (subtile_x % 4);
  const size_t subtile_idx = z_idx * 8 + in_z_idx;

  const size_t tile_x = x / kTile4TileWidthBytes;
  const size_t tile_y = y / kTile4TileHeightBytes;
  const size_t tiles_per_row = width / kTile4TileWidthBytes;
  const size_t tile_idx = tile_y * tiles_per_row + tile_x;

  return tile_idx * kTile4TileSizeBytes + subtile_idx * kTile4SubtileSizeBytes +
         offset_in_subtile;
}

uint8_t PixelPattern(size_t x, size_t y) {
  return static_cast<uint8_t>((x * 73 + y * 151 + (x ^ y)) & 0xff);
}

TEST(VaapiTestUtilsTest, Detile4SingleTile) {
  constexpr size_t kWidth = kTile4TileWidthBytes;
  constexpr size_t kHeight = kTile4TileHeightBytes;

  std::array<uint8_t, kWidth * kHeight> tiled;
  std::array<uint8_t, kWidth * kHeight> expected_linear;
  for (size_t y = 0; y < kHeight; ++y) {
    for (size_t x = 0; x < kWidth; ++x) {
      const uint8_t v = PixelPattern(x, y);
      expected_linear[y * kWidth + x] = v;
      tiled[Tile4Offset(x, y, kWidth)] = v;
    }
  }

  std::array<uint8_t, kWidth * kHeight> actual_linear;
  Detile4(actual_linear, tiled, kWidth, kHeight);
  EXPECT_EQ(actual_linear, expected_linear);
}

TEST(VaapiTestUtilsTest, Detile4MultipleTiles) {
  constexpr size_t kWidth = kTile4TileWidthBytes * 2;
  constexpr size_t kHeight = kTile4TileHeightBytes * 2;

  std::array<uint8_t, kWidth * kHeight> tiled;
  std::array<uint8_t, kWidth * kHeight> expected_linear;
  for (size_t y = 0; y < kHeight; ++y) {
    for (size_t x = 0; x < kWidth; ++x) {
      const uint8_t v = PixelPattern(x, y);
      expected_linear[y * kWidth + x] = v;
      tiled[Tile4Offset(x, y, kWidth)] = v;
    }
  }

  std::array<uint8_t, kWidth * kHeight> actual_linear;
  Detile4(actual_linear, tiled, kWidth, kHeight);
  EXPECT_EQ(actual_linear, expected_linear);
}

TEST(VaapiTestUtilsTest, Detile4UnalignedDimensions) {
  constexpr size_t kAlignedWidth = kTile4TileWidthBytes;
  constexpr size_t kAlignedHeight = kTile4TileHeightBytes;
  constexpr size_t kUnalignedWidth = kAlignedWidth + 16;
  constexpr size_t kUnalignedHeight = kAlignedHeight + 8;

  std::vector<uint8_t> tiled(kUnalignedWidth * kUnalignedHeight, 0xaa);
  std::vector<uint8_t> expected_linear(kUnalignedWidth * kUnalignedHeight,
                                       0xdd);
  for (size_t y = 0; y < kAlignedHeight; ++y) {
    for (size_t x = 0; x < kAlignedWidth; ++x) {
      const uint8_t v = PixelPattern(x, y);
      expected_linear[y * kAlignedWidth + x] = v;
      tiled[Tile4Offset(x, y, kAlignedWidth)] = v;
    }
  }

  std::vector<uint8_t> actual_linear(kUnalignedWidth * kUnalignedHeight, 0xdd);
  Detile4(actual_linear, tiled, kUnalignedWidth, kUnalignedHeight);
  EXPECT_EQ(actual_linear, expected_linear);
}

}  // namespace
}  // namespace media::vaapi_test_utils
