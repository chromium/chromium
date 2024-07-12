// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_searchify.h"

#include <numbers>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"

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
    EXPECT_FLOAT_EQ(-std::numbers::pi_v<float> / 4, result.theta);
  }

  {
    SearchifyBoundingBoxOrigin result = ConvertToPdfOriginForTesting(
        /*rect=*/kRect,
        /*angle=*/90,
        /*coordinate_system_height=*/792);
    EXPECT_EQ(gfx::PointF(70, 742), result.point);
    EXPECT_FLOAT_EQ(-std::numbers::pi_v<float> / 2, result.theta);
  }
  {
    SearchifyBoundingBoxOrigin result = ConvertToPdfOriginForTesting(
        /*rect=*/kRect,
        /*angle=*/180,
        /*coordinate_system_height=*/792);
    EXPECT_EQ(gfx::PointF(100, 772), result.point);
    EXPECT_FLOAT_EQ(-std::numbers::pi_v<float>, result.theta);
  }
  {
    SearchifyBoundingBoxOrigin result = ConvertToPdfOriginForTesting(
        /*rect=*/kRect,
        /*angle=*/-90,
        /*coordinate_system_height=*/792);
    EXPECT_EQ(gfx::PointF(130, 742), result.point);
    EXPECT_FLOAT_EQ(std::numbers::pi_v<float> / 2, result.theta);
  }
}

}  // namespace chrome_pdf
