// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_ink_cursor.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"

namespace chrome_pdf {

namespace {

TEST(PdfInkCursorTest, CursorDiameterFromBrushSizeAndZoom) {
  // Very small brush sizes result in a minimum cursor size.
  EXPECT_EQ(6, CursorDiameterFromBrushSizeAndZoom(/*brush_size=*/1.0f,
                                                  /*zoom=*/1.0f));
  EXPECT_EQ(6, CursorDiameterFromBrushSizeAndZoom(/*brush_size=*/2.0f,
                                                  /*zoom=*/1.0f));
  EXPECT_EQ(6, CursorDiameterFromBrushSizeAndZoom(/*brush_size=*/4.0f,
                                                  /*zoom=*/1.0f));

  // Small brush sizes have a small offset value.
  EXPECT_EQ(7, CursorDiameterFromBrushSizeAndZoom(/*brush_size=*/5.0f,
                                                  /*zoom=*/1.0f));
  EXPECT_EQ(8, CursorDiameterFromBrushSizeAndZoom(/*brush_size=*/6.0f,
                                                  /*zoom=*/1.0f));
  EXPECT_EQ(11, CursorDiameterFromBrushSizeAndZoom(/*brush_size=*/9.0f,
                                                   /*zoom=*/1.0f));

  // Larger brush sizes have a larger offset value.
  EXPECT_EQ(14, CursorDiameterFromBrushSizeAndZoom(/*brush_size=*/10.0f,
                                                   /*zoom=*/1.0f));
  EXPECT_EQ(15, CursorDiameterFromBrushSizeAndZoom(/*brush_size=*/11.0f,
                                                   /*zoom=*/1.0f));
  EXPECT_EQ(20, CursorDiameterFromBrushSizeAndZoom(/*brush_size=*/16.0f,
                                                   /*zoom=*/1.0f));

  // Very small brush sizes with zoom end up close to the minimum cursor size.
  EXPECT_EQ(6, CursorDiameterFromBrushSizeAndZoom(/*brush_size=*/1.0f,
                                                  /*zoom=*/0.25f));
  EXPECT_EQ(7, CursorDiameterFromBrushSizeAndZoom(/*brush_size=*/1.0f,
                                                  /*zoom=*/5.0f));

  // Small brush sizes scale with zoom, but can get clipped at min/max cursor
  // sizes.
  EXPECT_EQ(6, CursorDiameterFromBrushSizeAndZoom(/*brush_size=*/5.0f,
                                                  /*zoom=*/0.5f));
  EXPECT_EQ(19, CursorDiameterFromBrushSizeAndZoom(/*brush_size=*/5.0f,
                                                   /*zoom=*/3.0f));
  EXPECT_EQ(6, CursorDiameterFromBrushSizeAndZoom(/*brush_size=*/9.0f,
                                                  /*zoom=*/0.33f));
  EXPECT_EQ(32, CursorDiameterFromBrushSizeAndZoom(/*brush_size=*/9.0f,
                                                   /*zoom=*/4.0f));

  // Larger brush sizes scale with zoom, but often get clipped at max cursor
  // size.
  EXPECT_EQ(6, CursorDiameterFromBrushSizeAndZoom(/*brush_size=*/10.0f,
                                                  /*zoom=*/0.25f));
  EXPECT_EQ(24, CursorDiameterFromBrushSizeAndZoom(/*brush_size=*/10.0f,
                                                   /*zoom=*/2.0f));
  EXPECT_EQ(32, CursorDiameterFromBrushSizeAndZoom(/*brush_size=*/10.0f,
                                                   /*zoom=*/3.0f));
  EXPECT_EQ(32, CursorDiameterFromBrushSizeAndZoom(/*brush_size=*/16.0f,
                                                   /*zoom=*/5.0f));
}

TEST(PdfInkCursorTest, GenerateToolCursor) {
  // For each cursor bitmap, test:
  // 1) The top-left corner, which is the outline color for small bitmaps, and
  // transparent for larger bitmaps.
  // 2) The middle, which is the solid cursor color.
  // 3) The right edge, which is the outline color. Note that lighter cursor
  // colors have a darker outline color, and vice versa.
  {
    SkBitmap bitmap = GenerateToolCursor(SK_ColorBLACK, 6);
    ASSERT_EQ(6, bitmap.width());
    ASSERT_EQ(6, bitmap.height());
    ASSERT_FALSE(bitmap.drawsNothing());
    EXPECT_EQ(SkColorSetARGB(0x18, 0xAA, 0xAA, 0xAA), bitmap.getColor(0, 0));
    EXPECT_EQ(SK_ColorBLACK, bitmap.getColor(3, 3));
    EXPECT_EQ(SkColorSetARGB(0x18, 0xAA, 0xAA, 0xAA), bitmap.getColor(5, 0));
  }
  {
    SkBitmap bitmap = GenerateToolCursor(SK_ColorRED, 8);
    ASSERT_EQ(8, bitmap.width());
    ASSERT_EQ(8, bitmap.height());
    ASSERT_FALSE(bitmap.drawsNothing());
    EXPECT_EQ(SK_ColorTRANSPARENT, bitmap.getColor(0, 0));
    EXPECT_EQ(SK_ColorRED, bitmap.getColor(4, 4));
    EXPECT_EQ(SkColorSetARGB(0xF0, 0xAA, 0xAA, 0xAA), bitmap.getColor(7, 4));
  }
  {
    SkBitmap bitmap = GenerateToolCursor(SK_ColorWHITE, 20);
    ASSERT_EQ(20, bitmap.width());
    ASSERT_EQ(20, bitmap.height());
    ASSERT_FALSE(bitmap.drawsNothing());
    EXPECT_EQ(SK_ColorTRANSPARENT, bitmap.getColor(0, 0));
    EXPECT_EQ(SK_ColorWHITE, bitmap.getColor(10, 10));
    EXPECT_EQ(SkColorSetARGB(0xE8, 0x90, 0x90, 0x90), bitmap.getColor(19, 10));
  }
}

}  // namespace

}  // namespace chrome_pdf
