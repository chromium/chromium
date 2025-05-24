// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/skia_utils_base.h"

#include "base/containers/span.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkImageInfo.h"

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

}  // namespace
}  // namespace skia
