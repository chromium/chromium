// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_searchify.h"

#include <algorithm>
#include <ostream>

#include "base/numerics/angle_conversions.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"

namespace {

constexpr float kFloatTolerance = 0.00001f;

bool FloatNear(float a, float b, float abs_error) {
  return std::abs(a - b) <= abs_error;
}

}  // namespace

// Lets EXPECT_EQ() compare FS_MATRIX.
constexpr bool operator==(const FS_MATRIX& lhs, const FS_MATRIX& rhs) {
  return FloatNear(lhs.a, rhs.a, kFloatTolerance) &&
         FloatNear(lhs.b, rhs.b, kFloatTolerance) &&
         FloatNear(lhs.c, rhs.c, kFloatTolerance) &&
         FloatNear(lhs.d, rhs.d, kFloatTolerance) &&
         FloatNear(lhs.e, rhs.e, kFloatTolerance) &&
         FloatNear(lhs.f, rhs.f, kFloatTolerance);
}

// Lets EXPECT_EQ() automatically print out FS_MATRIX on failure. Similar to
// other PrintTo() functions in //ui/gfx/geometry.
void PrintTo(const FS_MATRIX& matrix, ::std::ostream* os) {
  *os << base::StringPrintf("%f,%f,%f,%f,%f,%f", matrix.a, matrix.b, matrix.c,
                            matrix.d, matrix.e, matrix.f);
}

namespace chrome_pdf {

TEST(PdfiumSearchifyTest, ConvertToPdfOrigin) {
  constexpr gfx::Rect kRect(100, 50, 20, 30);
  {
    SearchifyBoundingBoxOrigin result = ConvertToPdfOriginForTesting(
        /*rect=*/kRect,
        /*angle=*/0,
        /*coordinate_system_height=*/792);
    EXPECT_EQ(gfx::PointF(100, 712), result.point);
    EXPECT_FLOAT_EQ(0, result.theta);
  }

  {
    SearchifyBoundingBoxOrigin result = ConvertToPdfOriginForTesting(
        /*rect=*/kRect,
        /*angle=*/45,
        /*coordinate_system_height=*/792);
    EXPECT_EQ(gfx::PointF(78.786796f, 720.786796f), result.point);
    EXPECT_FLOAT_EQ(base::DegToRad<float>(-45), result.theta);
  }

  {
    SearchifyBoundingBoxOrigin result = ConvertToPdfOriginForTesting(
        /*rect=*/kRect,
        /*angle=*/90,
        /*coordinate_system_height=*/792);
    EXPECT_EQ(gfx::PointF(70, 742), result.point);
    EXPECT_FLOAT_EQ(base::DegToRad<float>(-90), result.theta);
  }
  {
    SearchifyBoundingBoxOrigin result = ConvertToPdfOriginForTesting(
        /*rect=*/kRect,
        /*angle=*/180,
        /*coordinate_system_height=*/792);
    EXPECT_EQ(gfx::PointF(100, 772), result.point);
    EXPECT_FLOAT_EQ(base::DegToRad<float>(-180), result.theta);
  }
  {
    SearchifyBoundingBoxOrigin result = ConvertToPdfOriginForTesting(
        /*rect=*/kRect,
        /*angle=*/-90,
        /*coordinate_system_height=*/792);
    EXPECT_EQ(gfx::PointF(130, 742), result.point);
    EXPECT_FLOAT_EQ(base::DegToRad<float>(90), result.theta);
  }
}

TEST(PdfiumSearchifyTest, CalculateWordMoveMatrix) {
  constexpr gfx::Rect kRect(100, 50, 20, 30);
  {
    // 0 degree case.
    const SearchifyBoundingBoxOrigin origin(gfx::PointF(100, 712), 0);
    FS_MATRIX matrix = CalculateWordMoveMatrixForTesting(origin, kRect.width(),
                                                         /*word_is_rtl=*/false);
    EXPECT_EQ(FS_MATRIX(1, 0, 0, 1, 100, 712), matrix);
    FS_MATRIX matrix_rtl =
        CalculateWordMoveMatrixForTesting(origin, kRect.width(),
                                          /*word_is_rtl=*/true);
    EXPECT_EQ(FS_MATRIX(-1, 0, 0, 1, 120, 712), matrix_rtl);
  }
  {
    // 45 degree case.
    const SearchifyBoundingBoxOrigin origin(
        gfx::PointF(78.786796f, 720.786796f), base::DegToRad<float>(-45));
    FS_MATRIX matrix = CalculateWordMoveMatrixForTesting(origin, kRect.width(),
                                                         /*word_is_rtl=*/false);
    EXPECT_EQ(FS_MATRIX(0.707107f, -0.707107f, 0.707107f, 0.707107f, 78.786797f,
                        720.786804f),
              matrix);
    FS_MATRIX matrix_rtl =
        CalculateWordMoveMatrixForTesting(origin, kRect.width(),
                                          /*word_is_rtl=*/true);
    EXPECT_EQ(FS_MATRIX(-0.707107f, 0.707107f, 0.707107f, 0.707107f, 92.928932f,
                        706.644653f),
              matrix_rtl);
  }
  {
    // 90 degree case.
    const SearchifyBoundingBoxOrigin origin(gfx::PointF(70, 742),
                                            base::DegToRad<float>(-90));
    FS_MATRIX matrix = CalculateWordMoveMatrixForTesting(origin, kRect.width(),
                                                         /*word_is_rtl=*/false);
    EXPECT_EQ(FS_MATRIX(0, -1, 1, 0, 70, 742), matrix);
    FS_MATRIX matrix_rtl =
        CalculateWordMoveMatrixForTesting(origin, kRect.width(),
                                          /*word_is_rtl=*/true);
    EXPECT_EQ(FS_MATRIX(0, 1, 1, 0, 70, 722), matrix_rtl);
  }
  {
    // -90 degree case.
    const SearchifyBoundingBoxOrigin origin(gfx::PointF(130, 742),
                                            base::DegToRad<float>(90));
    FS_MATRIX matrix = CalculateWordMoveMatrixForTesting(origin, kRect.width(),
                                                         /*word_is_rtl=*/false);
    EXPECT_EQ(FS_MATRIX(0, 1, -1, 0, 130, 742), matrix);
    FS_MATRIX matrix_rtl =
        CalculateWordMoveMatrixForTesting(origin, kRect.width(),
                                          /*word_is_rtl=*/true);
    EXPECT_EQ(FS_MATRIX(0, -1, -1, 0, 130, 762), matrix_rtl);
  }
}

}  // namespace chrome_pdf
