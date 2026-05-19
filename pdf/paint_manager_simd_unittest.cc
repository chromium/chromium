// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/paint_manager_simd.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "base/compiler_specific.h"
#include "base/rand_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_pdf {
namespace {

void ReferenceNonPremulBlend(base::span<const uint8_t> src,
                             base::span<uint8_t> dest) {
  size_t n_pixels = dest.size() / 4;
  for (size_t col_i = 0; col_i < n_pixels; ++col_i) {
    base::span<const uint8_t> src_bgra = src.subspan(col_i * 4, 4u);
    base::span<uint8_t> dest_bgra = dest.subspan(col_i * 4, 4u);
    uint8_t alpha = src_bgra[3];

    auto blend = [alpha](uint8_t src) {
      int prod = (static_cast<int>(src) - 255) * alpha + 128;
      prod = (prod + (prod >> 8)) >> 8;
      return static_cast<uint8_t>(255 + prod);
    };
    dest_bgra[0] = blend(src_bgra[0]);
    dest_bgra[1] = blend(src_bgra[1]);
    dest_bgra[2] = blend(src_bgra[2]);
    dest_bgra[3] = 255;
  }
}

void ReferencePremulBlend(base::span<const uint8_t> src,
                          base::span<uint8_t> dest) {
  size_t n_pixels = dest.size() / 4;
  for (size_t col_i = 0; col_i < n_pixels; ++col_i) {
    base::span<const uint8_t> src_bgra = src.subspan(col_i * 4, 4u);
    base::span<uint8_t> dest_bgra = dest.subspan(col_i * 4, 4u);
    uint8_t inv_alpha = 255 - src_bgra[3];
    dest_bgra[0] = src_bgra[0] + inv_alpha;
    dest_bgra[1] = src_bgra[1] + inv_alpha;
    dest_bgra[2] = src_bgra[2] + inv_alpha;
    dest_bgra[3] = 255;
  }
}

std::vector<uint8_t> GenerateTestBuffer() {
  std::vector<uint8_t> buffer(256 * 4);
  base::RandBytes(buffer);

  for (int i = 0; i < 256; ++i) {
    // Span the alpha channel's domain.
    buffer[i * 4 + 3] = i;
  }
  return buffer;
}

// Correctness Tests against Reference.

// Tests that `NonPremulBlend` produces bit-exact output compared to the
// scalar reference implementation.
TEST(PaintManagerSimdTest, NonPremulBlendIsCorrect) {
  std::vector<uint8_t> src = GenerateTestBuffer();
  size_t n_pixels = src.size() / 4;
  std::vector<uint8_t> actual_dest(n_pixels * 4);
  std::vector<uint8_t> expected_dest(n_pixels * 4);

  NonPremulBlend(src.data(), actual_dest.data(), n_pixels);
  ReferenceNonPremulBlend(src, expected_dest);

  EXPECT_EQ(actual_dest, expected_dest);
}

// Tests that `PremulBlend` produces bit-exact output compared to the
// scalar reference implementation.
TEST(PaintManagerSimdTest, PremulBlendIsCorrect) {
  std::vector<uint8_t> src = GenerateTestBuffer();
  // Clamp components to alpha to ensure valid premul input
  for (size_t i = 0; i < 256; ++i) {
    uint8_t a = src[i * 4 + 3];
    src[i * 4] = std::min(src[i * 4], a);
    src[i * 4 + 1] = std::min(src[i * 4 + 1], a);
    src[i * 4 + 2] = std::min(src[i * 4 + 2], a);
  }
  size_t n_pixels = src.size() / 4;
  std::vector<uint8_t> actual_dest(n_pixels * 4);
  std::vector<uint8_t> expected_dest(n_pixels * 4);

  PremulBlend(src.data(), actual_dest.data(), n_pixels);
  ReferencePremulBlend(src, expected_dest);

  EXPECT_EQ(actual_dest, expected_dest);
}

// From here onwards, the tests are "property tests", meaning that they test
// properties of outputs from blending.

// Property: Blending over a white background (`255`) must always produce
// color components greater than or equal to the source components
// (i.e., moving towards white).
TEST(PaintManagerSimdTest, NonPremulBlendBoundsProperty) {
  std::vector<uint8_t> src = GenerateTestBuffer();
  size_t n_pixels = src.size() / 4;
  std::vector<uint8_t> dest(n_pixels * 4);

  NonPremulBlend(src.data(), dest.data(), n_pixels);

  for (size_t i = 0; i < n_pixels; ++i) {
    EXPECT_GE(dest[i * 4], src[i * 4]);
    EXPECT_GE(dest[i * 4 + 1], src[i * 4 + 1]);
    EXPECT_GE(dest[i * 4 + 2], src[i * 4 + 2]);
    EXPECT_EQ(dest[i * 4 + 3], 255);
  }
}

// Property: Valid premultiplied inputs (color channels clamped to `<= alpha`)
// blended over white produce components `>=` source components and never wrap
// around due to integer overflow.
TEST(PaintManagerSimdTest, PremulBlendValidInputProperty) {
  std::vector<uint8_t> src = GenerateTestBuffer();
  // Clamp components to alpha to ensure valid premul input
  for (size_t i = 0; i < 256; ++i) {
    uint8_t a = src[i * 4 + 3];
    src[i * 4] = std::min(src[i * 4], a);
    src[i * 4 + 1] = std::min(src[i * 4 + 1], a);
    src[i * 4 + 2] = std::min(src[i * 4 + 2], a);
  }
  size_t n_pixels = src.size() / 4;
  std::vector<uint8_t> dest(n_pixels * 4);

  PremulBlend(src.data(), dest.data(), n_pixels);

  for (size_t i = 0; i < n_pixels; ++i) {
    EXPECT_GE(dest[i * 4], src[i * 4]);
    EXPECT_GE(dest[i * 4 + 1], src[i * 4 + 1]);
    EXPECT_GE(dest[i * 4 + 2], src[i * 4 + 2]);
    EXPECT_EQ(dest[i * 4 + 3], 255);
  }
}

// Property: Fully opaque source pixels (`A == 255`) must have their
// original color components preserved exactly in the destination buffer.
//
// Using templates to remove code duplication between the blending impls.
template <void (*BlendFunc)(uint8_t*, uint8_t*, size_t)>
void VerifyBlendOpaqueProperty() {
  std::vector<uint8_t> src = GenerateTestBuffer();
  size_t n_pixels = src.size() / 4;
  for (size_t i = 0; i < n_pixels; ++i) {
    src[i * 4 + 3] = 255;
  }
  std::vector<uint8_t> dest(n_pixels * 4);

  BlendFunc(src.data(), dest.data(), n_pixels);

  for (size_t i = 0; i < n_pixels; ++i) {
    EXPECT_EQ(dest[i * 4], src[i * 4]);
    EXPECT_EQ(dest[i * 4 + 1], src[i * 4 + 1]);
    EXPECT_EQ(dest[i * 4 + 2], src[i * 4 + 2]);
    EXPECT_EQ(dest[i * 4 + 3], 255);
  }
}

TEST(PaintManagerSimdTest, NonPremulBlendOpaqueProperty) {
  VerifyBlendOpaqueProperty<NonPremulBlend>();
}

TEST(PaintManagerSimdTest, PremulBlendOpaqueProperty) {
  VerifyBlendOpaqueProperty<PremulBlend>();
}

// Property: Fully transparent source pixels (`A == 0`) must render as pure
// white (`255, 255, 255, 255`).
template <void (*BlendFunc)(uint8_t*, uint8_t*, size_t), bool kIsPremul>
void VerifyBlendTransparentProperty() {
  std::vector<uint8_t> src = GenerateTestBuffer();
  size_t n_pixels = src.size() / 4;
  for (size_t i = 0; i < n_pixels; ++i) {
    src[i * 4 + 3] = 0;
    if (kIsPremul) {
      src[i * 4] = 0;
      src[i * 4 + 1] = 0;
      src[i * 4 + 2] = 0;
    }
  }
  std::vector<uint8_t> dest(n_pixels * 4);

  BlendFunc(src.data(), dest.data(), n_pixels);

  for (size_t i = 0; i < n_pixels; ++i) {
    EXPECT_EQ(dest[i * 4], 255);
    EXPECT_EQ(dest[i * 4 + 1], 255);
    EXPECT_EQ(dest[i * 4 + 2], 255);
    EXPECT_EQ(dest[i * 4 + 3], 255);
  }
}

TEST(PaintManagerSimdTest, NonPremulBlendTransparentProperty) {
  VerifyBlendTransparentProperty<NonPremulBlend, false>();
}

TEST(PaintManagerSimdTest, PremulBlendTransparentProperty) {
  VerifyBlendTransparentProperty<PremulBlend, true>();
}

// Property: The resulting destination buffer must always be fully opaque (`A ==
// 255`).
template <void (*BlendFunc)(uint8_t*, uint8_t*, size_t)>
void VerifyBlendOutputOpaqueProperty() {
  std::vector<uint8_t> src = GenerateTestBuffer();
  size_t n_pixels = src.size() / 4;
  std::vector<uint8_t> dest(n_pixels * 4);

  BlendFunc(src.data(), dest.data(), n_pixels);

  for (size_t i = 0; i < n_pixels; ++i) {
    EXPECT_EQ(dest[i * 4 + 3], 255);
  }
}

TEST(PaintManagerSimdTest, NonPremulBlendOutputOpaqueProperty) {
  VerifyBlendOutputOpaqueProperty<NonPremulBlend>();
}

TEST(PaintManagerSimdTest, PremulBlendOutputOpaqueProperty) {
  VerifyBlendOutputOpaqueProperty<PremulBlend>();
}

}  // namespace
}  // namespace chrome_pdf
