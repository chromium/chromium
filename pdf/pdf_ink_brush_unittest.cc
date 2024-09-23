// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_ink_brush.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"

namespace chrome_pdf {

namespace {

std::unique_ptr<chrome_pdf::PdfInkBrush> CreatePdfInkBrush(float size) {
  return std::make_unique<chrome_pdf::PdfInkBrush>(
      chrome_pdf::PdfInkBrush::Type::kPen,
      chrome_pdf::PdfInkBrush::Params{SK_ColorBLACK, size});
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

}  // namespace chrome_pdf
