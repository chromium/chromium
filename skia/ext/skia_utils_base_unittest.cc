// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/skia_utils_base.h"

#include <array>

#include "base/containers/span.h"
#include "base/strings/string_view_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixmap.h"

namespace skia {
namespace {

#define EXPECT_EQ_BITMAP(a, b)                               \
  do {                                                       \
    EXPECT_EQ(a.pixmap().addr(), b.pixmap().addr());         \
    EXPECT_EQ(a.pixmap().rowBytes(), b.pixmap().rowBytes()); \
    EXPECT_EQ(a.pixmap().info(), b.pixmap().info());         \
  } while (false)

TEST(SkiaUtilsBase, ConvertNullToN32) {
  SkBitmap bitmap;
  SkBitmap out;
  EXPECT_TRUE(SkBitmapToN32OpaqueOrPremul(bitmap, &out));
  // Returned a copy of the input bitmap.
  EXPECT_EQ_BITMAP(bitmap, out);
}

TEST(SkiaUtilsBase, ConvertValidToN32) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(10, 12);
  SkBitmap out;
  EXPECT_TRUE(SkBitmapToN32OpaqueOrPremul(bitmap, &out));
  // Returned a copy of the input bitmap.
  EXPECT_EQ_BITMAP(bitmap, out);
}

TEST(SkiaUtilsBase, ConvertWeirdStrideToN32) {
  int width = 10;
  int height = 12;

  SkBitmap bitmap;
  // Stride is > 4 * width.
  bitmap.allocPixels(SkImageInfo::MakeN32(width, height, kPremul_SkAlphaType),
                     width * 4 + 4);

  SkBitmap out;
  EXPECT_TRUE(SkBitmapToN32OpaqueOrPremul(bitmap, &out));
  // The stride was converted.
  EXPECT_NE(bitmap.rowBytes(), out.rowBytes());
  EXPECT_EQ(out.rowBytes(), width * 4u);
}

TEST(SkiaUtilsBase, ConvertWeirdFormatToN32) {
  int width = 10;
  int height = 12;

  // A format smaller than N32.
  {
    SkBitmap bitmap;
    bitmap.allocPixels(SkImageInfo::MakeA8(width, height));

    SkBitmap out;
    EXPECT_TRUE(SkBitmapToN32OpaqueOrPremul(bitmap, &out));
    // The format was converted.
    EXPECT_NE(bitmap.rowBytes(), out.rowBytes());
    EXPECT_NE(bitmap.info().colorType(), out.info().colorType());
    EXPECT_EQ(out.rowBytes(), width * 4u);
    EXPECT_EQ(out.info().colorType(), kN32_SkColorType);
  }

  // A format larger than N32.
  {
    SkBitmap bitmap;
    bitmap.allocPixels(SkImageInfo::Make(width, height, kRGBA_F16_SkColorType,
                                         kPremul_SkAlphaType));

    SkBitmap out;
    EXPECT_TRUE(SkBitmapToN32OpaqueOrPremul(bitmap, &out));
    // The format was converted.
    EXPECT_NE(bitmap.rowBytes(), out.rowBytes());
    EXPECT_NE(bitmap.info().colorType(), out.info().colorType());
    EXPECT_EQ(out.rowBytes(), width * 4u);
    EXPECT_EQ(out.info().colorType(), kN32_SkColorType);
  }
}

TEST(SkiaUtilsBase, ConvertSkColorToHexString) {
  EXPECT_EQ(SkColorToHexString(SK_ColorBLUE), "#0000FF");
  EXPECT_EQ(SkColorToHexString(SK_ColorRED), "#FF0000");
  EXPECT_EQ(SkColorToHexString(SK_ColorGREEN), "#00FF00");
  EXPECT_EQ(SkColorToHexString(SK_ColorWHITE), "#FFFFFF");
}

TEST(SkiaUtilsBase, ConvertSkDataToByteSpan) {
  sk_sp<SkData> sk_data = SkData::MakeWithCString("foobar");
  base::span<const uint8_t> span = as_byte_span(*sk_data);
  EXPECT_EQ(span.size(), 6u /* "foobar" */ + 1u /* NUL character */);
  EXPECT_EQ(0, span[6]);
  EXPECT_EQ("foobar", base::as_string_view(span.first(6u)));
}

TEST(SkiaUtilsBase, ConvertSkDataToWritableByteSpan) {
  sk_sp<SkData> sk_data = SkData::MakeWithCString("foobar");
  base::span<uint8_t> span = as_writable_byte_span(*sk_data);
  EXPECT_EQ(span.size(), 6u /* "foobar" */ + 1u /* NUL character */);
  EXPECT_EQ(0, span[6]);
  span[3] = 't';
  span[4] = 'e';
  EXPECT_EQ("footer", base::as_string_view(as_byte_span(*sk_data).first(6u)));
}

TEST(SkiaUtilsBase, ConvertSkPixmapToByteSpan) {
  // A 3x2 kRGBA_8888 image with 16 bytes per row. The span ends after the last
  // pixel of the last row, excluding that row's padding.
  constexpr size_t kRowBytes = 16;
  std::array<uint8_t, 2 * kRowBytes> pixels = {};
  const SkPixmap pixmap(
      SkImageInfo::Make(3, 2, kRGBA_8888_SkColorType, kPremul_SkAlphaType),
      pixels.data(), kRowBytes);

  base::span<const uint8_t> span = as_byte_span(pixmap);
  EXPECT_EQ(span.data(), pixels.data());
  EXPECT_EQ(span.size(), kRowBytes + 3 * 4);

  base::span<uint8_t> writable_span = as_writable_byte_span(pixmap);
  EXPECT_EQ(writable_span.data(), pixels.data());
  EXPECT_EQ(writable_span.size(), kRowBytes + 3 * 4);
  writable_span[kRowBytes] = 42;
  EXPECT_EQ(pixels[kRowBytes], 42);
}

TEST(SkiaUtilsBase, ConvertEmptySkPixmapToByteSpan) {
  EXPECT_TRUE(as_byte_span(SkPixmap()).empty());
  EXPECT_TRUE(as_writable_byte_span(SkPixmap()).empty());

  // A pixmap with dimensions but no pixels.
  const SkPixmap no_pixels(SkImageInfo::MakeN32Premul(3, 2), nullptr, 12);
  EXPECT_TRUE(as_byte_span(no_pixels).empty());
  EXPECT_TRUE(as_writable_byte_span(no_pixels).empty());
}

TEST(SkiaUtilsBase, MakeSkPixmap) {
  const SkImageInfo info =
      SkImageInfo::Make(3, 2, kRGBA_8888_SkColorType, kPremul_SkAlphaType);
  constexpr size_t kRowBytes = 16;
  std::array<uint8_t, 2 * kRowBytes> pixels = {};

  // Default row bytes.
  std::optional<SkPixmap> pixmap = MakeSkPixmap(info, pixels);
  ASSERT_TRUE(pixmap);
  EXPECT_EQ(pixmap->info(), info);
  EXPECT_EQ(pixmap->addr(), pixels.data());
  EXPECT_EQ(pixmap->rowBytes(), info.minRowBytes());

  // Explicit row bytes. The last row's padding need not be present.
  pixmap = MakeSkPixmap(info, base::span(pixels).first(kRowBytes + 3 * 4),
                        kRowBytes);
  ASSERT_TRUE(pixmap);
  EXPECT_EQ(pixmap->addr(), pixels.data());
  EXPECT_EQ(pixmap->rowBytes(), kRowBytes);

  // The span is too small.
  EXPECT_FALSE(MakeSkPixmap(
      info, base::span(pixels).first(kRowBytes + 3 * 4 - 1), kRowBytes));

  // Row bytes smaller than the width.
  EXPECT_FALSE(MakeSkPixmap(info, pixels, 3 * 4 - 4));

  // Row bytes that are not a multiple of the pixel size.
  EXPECT_FALSE(MakeSkPixmap(info, pixels, 13));

  // Row bytes so large that the size overflows.
  EXPECT_FALSE(MakeSkPixmap(info, pixels, SIZE_MAX / 4 * 4));
}

}  // namespace
}  // namespace skia
