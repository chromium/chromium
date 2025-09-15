// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/canvas_interventions/noise_helper.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/containers/span_writer.h"
#include "base/test/gtest_util.h"
#include "net/base/schemeful_site.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/fingerprinting_protection/noise_token.h"
#include "third_party/blink/renderer/core/canvas_interventions/noise_hash.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace blink {
namespace {

using NoiseHelperTest = testing::Test;

std::vector<uint8_t> GetRandomPixels(uint32_t width, uint32_t height) {
  std::vector<uint8_t> pixels;
  uint32_t num_bytes = width * height * 4;
  pixels.resize(num_bytes);
  auto out = base::span<uint8_t>(pixels);
  auto writer = base::SpanWriter(out);
  // Using FNV as a pseudo-random generator.
  uint64_t seed = 0xcbf29ce484222325;
  for (uint32_t i = 0u; i < num_bytes; i += 8u) {
    writer.WriteU64LittleEndian(seed);
    seed *= 0x00000100000001B3;
  }
  return pixels;
}

TEST_F(NoiseHelperTest, NoisePixels) {
  const uint32_t width = 50u;
  const uint32_t height = 150u;
  std::vector<uint8_t> image_data = GetRandomPixels(width, height);
  base::span pixels(image_data);
  std::vector<uint8_t> image_data_orig;
  image_data_orig.resize(image_data.size());
  base::span<uint8_t> pixels_orig(image_data_orig);
  pixels_orig.copy_from(pixels);
  EXPECT_EQ(pixels, pixels_orig);

  // When noised, the pixels should be perturbed by at most kMaxNoisePerChannel.
  const NoiseToken token(0x01234678901234567);
  const auto token_hash = NoiseHash(token);
  NoisePixels(token_hash, pixels, width, height);
  EXPECT_NE(pixels, pixels_orig);
  double num_diff = 0;
  for (size_t i = 0; i < pixels.size(); ++i) {
    auto diff = std::max(pixels[i], pixels_orig[i]) -
                std::min(pixels[i], pixels_orig[i]);
    EXPECT_LE(diff, 3);
    if (diff != 0) {
      ++num_diff;
    }
  }
  // On average noise should be added to ~6/7 (=85.71%) channel values; for
  // convenience, ensuring it's higher than 50%.
  double pct_diff = num_diff / pixels.size();
  EXPECT_GT(pct_diff, 0.5);

  // Hashing again with the same token and site should result in the same noise.
  std::vector<uint8_t> image_data2;
  image_data2.resize(image_data_orig.size());
  base::span<uint8_t> pixels2(image_data2);
  pixels2.copy_from(pixels_orig);
  NoisePixels(token_hash, pixels2, width, height);
  EXPECT_EQ(pixels, pixels2);

  // Using a different token hash should result in different noise being added.
  const NoiseToken other_token(0x02234561728192389);
  const auto token_hash2 = NoiseHash(other_token);
  pixels2.copy_from(pixels_orig);
  NoisePixels(token_hash2, pixels2, width, height);
  EXPECT_NE(pixels, pixels2);
}

TEST_F(NoiseHelperTest, NoisePixelsAllSameValue) {
  const int width = 100;
  const int height = 100;
  const uint8_t channel_value = 50;
  std::array<uint8_t, width * height * 4> pixel_arr;
  std::ranges::fill(pixel_arr, channel_value);
  base::span<uint8_t> pixels(pixel_arr);
  const NoiseToken token(0x01234678901234567);
  auto token_hash = NoiseHash(token);
  std::array<uint8_t, 4> first_pixel;
  std::ranges::fill(first_pixel, channel_value);
  // It's possible that the first pixel remains unaltered (when noise for the 4
  // channels is 0).
  do {
    NoisePixels(token_hash, pixels, width, height);
    token_hash.Update(0x9876543210);
  } while (pixels.first(4u) == first_pixel);

  base::span<uint8_t> first_noised_pixel(first_pixel);
  first_noised_pixel.copy_from(pixels.first<4>());
  for (int i = 4; i < static_cast<int>(pixels.size()); i += 4) {
    EXPECT_EQ(pixels.subspan(static_cast<uint32_t>(i), 4u), first_noised_pixel);
  }
}

TEST_F(NoiseHelperTest, NoisePixelsVerticalStripes) {
  const size_t width = 16u;
  const size_t height = 16u;
  const NoiseToken token(0x01234678901234567);
  auto token_hash = NoiseHash(token);

  const std::vector<uint8_t> image_data_orig = GetRandomPixels(width, height);
  const base::span pixels_orig(image_data_orig);
  std::vector<uint8_t> image_data(image_data_orig.size());
  base::span pixels(image_data);

  for (size_t x = 0u; x < width; ++x) {
    // For each column, copy the original image and create a vertical stripe.
    pixels.copy_from(pixels_orig);
    for (size_t y = 0u; y < height; ++y) {
      // Fill the stripe with the same value (avoiding the empty pixel).
      std::ranges::fill(pixels.subspan((x + y * width) * 4, 4u), x + 1);
    }
    // When noised, the vertical stripe should have the same color.
    NoisePixels(token_hash, pixels, width, height);
    for (size_t y = 1u; y < height; ++y) {
      EXPECT_EQ(pixels.subspan(x * 4, 4u),
                pixels.subspan((x + y * width) * 4, 4u));
    }
  }
}

TEST_F(NoiseHelperTest, NoisePixelsHorizontalStripes) {
  const size_t width = 16u;
  const size_t height = 16u;
  const NoiseToken token(0x01234678901234567);
  auto token_hash = NoiseHash(token);

  const std::vector<uint8_t> image_data_orig = GetRandomPixels(width, height);
  const base::span pixels_orig(image_data_orig);
  std::vector<uint8_t> image_data(image_data_orig.size());
  base::span pixels(image_data);

  for (size_t y = 0u; y < height; ++y) {
    // For each row, copy the original image and create a horizontal stripe.
    pixels.copy_from(pixels_orig);
    // Fill the stripe with the same value (avoiding the empty pixel).
    std::ranges::fill(pixels.subspan(y * width * 4, width * 4), y + 1);
    // When noised, the horizontal stripe should have the same color.
    NoisePixels(token_hash, pixels, width, height);
    for (size_t x = 1; x < width; ++x) {
      EXPECT_EQ(pixels.subspan(y * width * 4, 4u),
                pixels.subspan((x + y * width) * 4, 4u));
    }
  }
}

TEST_F(NoiseHelperTest, NoisePixelsSingleNeighbor) {
  const int width = 3;
  const int height = 3;
  const uint8_t val_default = 50;
  const uint8_t val_other = 150;
  std::array<uint8_t, width * height * 4> pixel_arr_orig;
  std::ranges::fill(pixel_arr_orig, val_default);
  std::array<uint8_t, width * height * 4> pixel_arr = pixel_arr_orig;
  base::span<uint8_t> pixels(pixel_arr);

  std::map<std::pair<size_t, size_t>, std::pair<size_t, size_t>>
      changed_to_checked = {
          {{0, 0}, {1, 1}},  // top-left
          {{0, 0}, {0, 1}},  // top
          {{1, 0}, {0, 1}},  // top-right
          {{1, 1}, {2, 1}}   // left
      };

  for (const auto& [changed, checked] : changed_to_checked) {
    pixels.copy_from(pixel_arr_orig);
    const NoiseToken token(0x01234678901234567);
    auto token_hash = NoiseHash(token);
    auto changed_pixel =
        pixels.subspan((changed.first + changed.second * width) * 4, 4u);
    auto checked_pixel =
        pixels.subspan((checked.first + checked.second * width) * 4, 4u);
    std::ranges::fill(changed_pixel, val_other);
    std::ranges::fill(checked_pixel, val_other);
    std::array<uint8_t, 4u> initial_pixel_value;
    std::ranges::fill(initial_pixel_value, val_other);
    // It's possible that the first pixel remains unaltered (when noise for the
    // 4 channels is 0).
    do {
      NoisePixels(token_hash, pixels, width, height);
      token_hash.Update(0x9876543210);
    } while (changed_pixel == initial_pixel_value);

    EXPECT_EQ(changed_pixel, checked_pixel);
    for (size_t y = 0; y < height; ++y) {
      for (size_t x = 0; x < width; ++x) {
        auto cur_pixel = pixels.subspan((x + y * width) * 4, 4u);
        for (int i = 0; i < 4; ++i) {
          if ((x == changed.first && y == changed.second) ||
              (x == checked.first && y == checked.second)) {
            // Ensure that the changed pixels have the correct values.
            EXPECT_GE(cur_pixel[i], val_other - 3);
            EXPECT_LE(cur_pixel[i], val_other + 3);
          } else {
            // Ensure that the other pixels did not change.
            EXPECT_GE(cur_pixel[i], val_default - 3);
            EXPECT_LE(cur_pixel[i], val_default + 3);
          }
        }
      }
    }
  }
}

TEST_F(NoiseHelperTest, NoisePixelsAlphaNonZero) {
  const uint32_t width = 50u;
  const uint32_t height = 150u;
  std::vector<uint8_t> image_data = GetRandomPixels(width, height);
  // Set alpha channel to 1.
  for (size_t i = 3; i < image_data.size(); i += 4) {
    image_data[i] = 1;
  }

  base::span pixels(image_data);
  std::vector<uint8_t> image_data_orig;
  image_data_orig.resize(image_data.size());
  base::span<uint8_t> pixels_orig(image_data_orig);
  pixels_orig.copy_from(pixels);
  EXPECT_EQ(pixels, pixels_orig);

  // When noised, the alpha channel should remain > 0.
  const NoiseToken token(0x01234678901234567);
  const auto token_hash = NoiseHash(token);
  NoisePixels(token_hash, pixels, width, height);
  EXPECT_NE(pixels, pixels_orig);
  ASSERT_EQ(pixels.size(), pixels_orig.size());
  int num_zero_noised = 0;
  for (size_t i = 3; i < pixels.size(); i += 4) {
    if (pixels[i] == 0) {
      ++num_zero_noised;
    }
  }
  EXPECT_EQ(num_zero_noised, 0);
}

}  // namespace
}  // namespace blink
