// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_ink_brush.h"

#include <memory>

#include "pdf/pdf_ink_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"

namespace chrome_pdf {

namespace {

std::unique_ptr<PdfInkBrush> CreatePdfInkBrush(float size) {
  return std::make_unique<PdfInkBrush>(PdfInkBrush::Type::kPen, SK_ColorBLACK,
                                       size);
}

}  // namespace

TEST(PdfInkBrushTest, InvalidateSinglePoint) {
  constexpr gfx::PointF kPoint(40.0f, 16.0f);
  auto brush = CreatePdfInkBrush(/*size=*/10.0f);
  EXPECT_EQ(gfx::Rect(35, 11, 10, 10),
            brush->GetInvalidateArea(kPoint, kPoint));
}

TEST(PdfInkBrushTest, InvalidateSinglePointNearBorder) {
  // Using a point closer to the border than the radius of the brush results in
  // the invalidation area including a negative origin.
  constexpr gfx::PointF kPoint(3.0f, 4.0f);
  auto brush = CreatePdfInkBrush(/*size=*/13.0f);
  EXPECT_EQ(gfx::Rect(-4, -3, 14, 14),
            brush->GetInvalidateArea(kPoint, kPoint));
}

TEST(PdfInkBrushTest, InvalidateDifferentPoints) {
  auto brush = CreatePdfInkBrush(/*size=*/10.0f);
  EXPECT_EQ(gfx::Rect(10, 11, 35, 26),
            brush->GetInvalidateArea(gfx::PointF(40.0f, 16.0f),
                                     gfx::PointF(15.0f, 32.0f)));
}

TEST(PdfInkBrushTest, SetColor) {
  auto brush = CreatePdfInkBrush(/*size=*/10.0f);
  EXPECT_EQ(SK_ColorBLACK, GetSkColorFromInkBrush(brush->ink_brush()));

  brush->SetColor(SK_ColorCYAN);
  EXPECT_EQ(SK_ColorCYAN, GetSkColorFromInkBrush(brush->ink_brush()));
}

TEST(PdfInkBrushTest, SetSize) {
  auto brush = CreatePdfInkBrush(/*size=*/10.0f);
  EXPECT_EQ(10.0f, brush->ink_brush().GetSize());

  brush->SetSize(4.0f);
  EXPECT_EQ(4.0f, brush->ink_brush().GetSize());
}

}  // namespace chrome_pdf
