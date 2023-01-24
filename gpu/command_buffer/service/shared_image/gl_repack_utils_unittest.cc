// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/gl_repack_utils.h"

#include "base/strings/stringprintf.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/pixel_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"

namespace gpu {
namespace {

static SkBitmap MakeSolidColorBitmap(const gfx::Size& size,
                                     SkColorType color_type,
                                     SkColor color) {
  SkBitmap bitmap;
  SkImageInfo info = SkImageInfo::Make(size.width(), size.height(), color_type,
                                       kUnpremul_SkAlphaType);
  bitmap.allocPixels(info);
  bitmap.eraseColor(color);
  return bitmap;
}

static SkBitmap MakeSolidColorBitmapWithStride(const gfx::Size& size,
                                               SkColorType color_type,
                                               SkColor color,
                                               size_t stride) {
  SkBitmap bitmap;
  SkImageInfo info = SkImageInfo::Make(size.width(), size.height(), color_type,
                                       kUnpremul_SkAlphaType);
  bitmap.allocPixels(info, stride);
  bitmap.eraseColor(color);
  return bitmap;
}

// Validates `repacked_data` has all red RGB pixels.
static void ValidateRgbPixelsRed(const gfx::Size& size,
                                 size_t expected_stride,
                                 const std::vector<uint8_t>& repacked_data) {
  size_t expected_length = expected_stride * size.height();
  ASSERT_EQ(repacked_data.size(), expected_length);

  for (int y = 0; y < size.height(); ++y) {
    for (int x = 0; x < size.width(); ++x) {
      size_t start = expected_stride * y + x * 3;
      bool is_red = repacked_data[start] == 255 &&
                    repacked_data[start + 1] == 0 &&
                    repacked_data[start + 2] == 0;
      ASSERT_TRUE(is_red) << base::StringPrintf(
          "Pixel (%d,%d) is RGB(%02X,%02X,%02X)", x, y, repacked_data[start],
          repacked_data[start + 1], repacked_data[start + 2]);
    }
  }
}

// Validates `repacked_data` has all red opaque RGBA pixels.
static void ValidateRgbaPixelsRed(const gfx::Size& size,
                                  size_t expected_stride,
                                  const std::vector<uint8_t>& repacked_data) {
  size_t expected_length = expected_stride * size.height();
  ASSERT_EQ(repacked_data.size(), expected_length);

  for (int y = 0; y < size.height(); ++y) {
    for (int x = 0; x < size.width(); ++x) {
      size_t start = expected_stride * y + x * 4;
      bool is_red =
          repacked_data[start] == 255 && repacked_data[start + 1] == 0 &&
          repacked_data[start + 2] == 0 && repacked_data[start + 3] == 255;
      ASSERT_TRUE(is_red) << base::StringPrintf(
          "Pixel (%d,%d) is RGBA(%02X,%02X,%02X,%02X)", x, y,
          repacked_data[start], repacked_data[start + 1],
          repacked_data[start + 2], repacked_data[start + 3]);
    }
  }
}

TEST(RepackUtilsTest, BgrxAsRgb) {
  constexpr gfx::Size size(10, 10);
  SkBitmap bitmap =
      MakeSolidColorBitmap(size, kBGRA_8888_SkColorType, SK_ColorRED);

  auto repacked_data =
      RepackPixelDataAsRgb(size, bitmap.pixmap(), /*src_is_bgrx=*/true);

  // Stride should be 10*3 = 30 aligned to 4 bytes so 32.
  constexpr size_t expected_stride = 32;

  ValidateRgbPixelsRed(size, expected_stride, repacked_data);
}

TEST(RepackUtilsTest, BgrxAsRgbWithAlignedStride) {
  // Repacked RGB stride is already 4 byte aligned 12*3=36.
  constexpr gfx::Size size(12, 12);
  SkBitmap bitmap =
      MakeSolidColorBitmap(size, kBGRA_8888_SkColorType, SK_ColorRED);

  auto repacked_data =
      RepackPixelDataAsRgb(size, bitmap.pixmap(), /*src_is_bgrx=*/true);

  // Stride should be 12*3 = 36 which is aligned to 4 bytes.
  constexpr size_t expected_stride = 36;

  ValidateRgbPixelsRed(size, expected_stride, repacked_data);
}

TEST(RepackUtilsTest, BgrxAsRgbWithAlpha) {
  constexpr gfx::Size size(10, 10);

  // Bitmap has alpha value that isn't opaque. This should be ignored since it's
  // actually BGRX not BGRA.
  SkColor color = SkColorSetARGB(128, 255, 0, 0);
  SkBitmap bitmap = MakeSolidColorBitmap(size, kBGRA_8888_SkColorType, color);

  auto repacked_data =
      RepackPixelDataAsRgb(size, bitmap.pixmap(), /*src_is_bgrx=*/true);

  // Stride should be 10*3 = 30 aligned to 4 bytes so 32.
  constexpr size_t expected_stride = 32;

  ValidateRgbPixelsRed(size, expected_stride, repacked_data);
}

TEST(RepackUtilsTest, BgrxAsRgbWithStrideMismatch) {
  constexpr gfx::Size size(10, 10);

  // 10*4 = 40 is the minimum stride for source image but use a larger stride.
  constexpr size_t src_stride = 48;
  SkBitmap bitmap = MakeSolidColorBitmapWithStride(size, kBGRA_8888_SkColorType,
                                                   SK_ColorRED, src_stride);

  auto repacked_data =
      RepackPixelDataAsRgb(size, bitmap.pixmap(), /*src_is_bgrx=*/true);

  // RGB stride should be 10*3 = 30 aligned to 4 bytes so 32.
  constexpr size_t expected_stride = 32;

  ValidateRgbPixelsRed(size, expected_stride, repacked_data);
}

TEST(RepackUtilsTest, RgbxAsRgb) {
  constexpr gfx::Size size(10, 10);
  SkBitmap bitmap =
      MakeSolidColorBitmap(size, kRGBA_8888_SkColorType, SK_ColorRED);

  auto repacked_data =
      RepackPixelDataAsRgb(size, bitmap.pixmap(), /*src_is_bgrx=*/false);

  // Stride should be 10*3 = 30 aligned to 4 bytes so 32.
  constexpr size_t expected_stride = 32;

  ValidateRgbPixelsRed(size, expected_stride, repacked_data);
}

TEST(RepackUtilsTest, RepackStride) {
  constexpr gfx::Size size(10, 10);
  // RGBA stride should be 10*4 = 40 but make src bitmap stride larger.
  constexpr size_t expected_stride = 40;
  constexpr size_t src_stride = 48;

  SkBitmap bitmap = MakeSolidColorBitmapWithStride(size, kRGBA_8888_SkColorType,
                                                   SK_ColorRED, src_stride);

  auto repacked_data =
      RepackPixelDataWithStride(size, bitmap.pixmap(), expected_stride);

  ValidateRgbaPixelsRed(size, expected_stride, repacked_data);
}

TEST(RepackUtilsTest, UnpackStride) {
  constexpr gfx::Size size(10, 10);
  // RGBA stride should be 10*4 = 40 but make src bitmap stride larger.
  constexpr size_t expected_stride = 40;
  constexpr size_t src_stride = 48;

  SkBitmap source_bitmap = MakeSolidColorBitmapWithStride(
      size, kRGBA_8888_SkColorType, SK_ColorRED, src_stride);

  auto repacked_data =
      RepackPixelDataWithStride(size, source_bitmap.pixmap(), expected_stride);

  // Result starts with green pixels.
  SkBitmap result_bitmap = MakeSolidColorBitmapWithStride(
      size, kRGBA_8888_SkColorType, SK_ColorGREEN, src_stride);

  SkPixmap pixmap;
  ASSERT_TRUE(result_bitmap.peekPixels(&pixmap));

  // Result bitmap should have red pixels after.
  UnpackPixelDataWithStride(size, repacked_data, expected_stride, pixmap);

  EXPECT_TRUE(cc::MatchesBitmap(result_bitmap, source_bitmap,
                                cc::ExactPixelComparator()));
}

TEST(RepackUtilsTest, SwizzleRedAndBlue) {
  constexpr gfx::Size size(10, 10);
  SkBitmap swizzled_bitmap =
      MakeSolidColorBitmap(size, kRGBA_8888_SkColorType, SK_ColorRED);

  SkPixmap pixmap;
  ASSERT_TRUE(swizzled_bitmap.peekPixels(&pixmap));

  SwizzleRedAndBlue(pixmap);

  SkBitmap expected_bitmap =
      MakeSolidColorBitmap(size, kRGBA_8888_SkColorType, SK_ColorBLUE);

  EXPECT_TRUE(cc::MatchesBitmap(swizzled_bitmap, expected_bitmap,
                                cc::ExactPixelComparator()));
}

}  // namespace
}  // namespace gpu
