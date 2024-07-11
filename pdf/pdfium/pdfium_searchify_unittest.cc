// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_searchify.h"

#include <numbers>

#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_pdf {

TEST(PdfiumSearchifyTest, ConvertToPdfOrigin) {
  {
    SearchifyBoundingBoxOrigin result = ConvertToPdfOriginForTesting(
        /*x=*/100,
        /*y=*/50,
        /*height=*/30,
        /*angle=*/0,
        /*coordinate_system_height=*/792);
    EXPECT_FLOAT_EQ(100, result.x);
    EXPECT_FLOAT_EQ(712, result.y);
    EXPECT_FLOAT_EQ(0, result.theta);
  }

  {
    SearchifyBoundingBoxOrigin result = ConvertToPdfOriginForTesting(
        /*x=*/100,
        /*y=*/50,
        /*height=*/30,
        /*angle=*/45,
        /*coordinate_system_height=*/792);
    EXPECT_FLOAT_EQ(78.786796f, result.x);
    EXPECT_FLOAT_EQ(720.786796f, result.y);
    EXPECT_FLOAT_EQ(-std::numbers::pi_v<float> / 4, result.theta);
  }

  {
    SearchifyBoundingBoxOrigin result = ConvertToPdfOriginForTesting(
        /*x=*/100,
        /*y=*/50,
        /*height=*/30,
        /*angle=*/90,
        /*coordinate_system_height=*/792);
    EXPECT_FLOAT_EQ(70, result.x);
    EXPECT_FLOAT_EQ(742, result.y);
    EXPECT_FLOAT_EQ(-std::numbers::pi_v<float> / 2, result.theta);
  }
  {
    SearchifyBoundingBoxOrigin result = ConvertToPdfOriginForTesting(
        /*x=*/100,
        /*y=*/50,
        /*height=*/30,
        /*angle=*/180,
        /*coordinate_system_height=*/792);
    EXPECT_FLOAT_EQ(100, result.x);
    EXPECT_FLOAT_EQ(772, result.y);
    EXPECT_FLOAT_EQ(-std::numbers::pi_v<float>, result.theta);
  }
  {
    SearchifyBoundingBoxOrigin result = ConvertToPdfOriginForTesting(
        /*x=*/100,
        /*y=*/50,
        /*height=*/30,
        /*angle=*/-90,
        /*coordinate_system_height=*/792);
    EXPECT_FLOAT_EQ(130, result.x);
    EXPECT_FLOAT_EQ(742, result.y);
    EXPECT_FLOAT_EQ(std::numbers::pi_v<float> / 2, result.theta);
  }
}

}  // namespace chrome_pdf
