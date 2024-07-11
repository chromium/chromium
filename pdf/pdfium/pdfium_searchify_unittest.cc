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
    EXPECT_DOUBLE_EQ(100, result.x);
    EXPECT_DOUBLE_EQ(712, result.y);
    EXPECT_DOUBLE_EQ(0, result.theta);
  }

  {
    SearchifyBoundingBoxOrigin result = ConvertToPdfOriginForTesting(
        /*x=*/100,
        /*y=*/50,
        /*height=*/30,
        /*angle=*/45,
        /*coordinate_system_height=*/792);
    EXPECT_DOUBLE_EQ(78.786796564403573, result.x);
    EXPECT_DOUBLE_EQ(720.78679656440363, result.y);
    EXPECT_DOUBLE_EQ(-std::numbers::pi / 4, result.theta);
  }

  {
    SearchifyBoundingBoxOrigin result = ConvertToPdfOriginForTesting(
        /*x=*/100,
        /*y=*/50,
        /*height=*/30,
        /*angle=*/90,
        /*coordinate_system_height=*/792);
    EXPECT_DOUBLE_EQ(70, result.x);
    EXPECT_DOUBLE_EQ(742, result.y);
    EXPECT_DOUBLE_EQ(-std::numbers::pi / 2, result.theta);
  }
  {
    SearchifyBoundingBoxOrigin result = ConvertToPdfOriginForTesting(
        /*x=*/100,
        /*y=*/50,
        /*height=*/30,
        /*angle=*/180,
        /*coordinate_system_height=*/792);
    EXPECT_DOUBLE_EQ(100, result.x);
    EXPECT_DOUBLE_EQ(772, result.y);
    EXPECT_DOUBLE_EQ(-std::numbers::pi, result.theta);
  }
  {
    SearchifyBoundingBoxOrigin result = ConvertToPdfOriginForTesting(
        /*x=*/100,
        /*y=*/50,
        /*height=*/30,
        /*angle=*/-90,
        /*coordinate_system_height=*/792);
    EXPECT_DOUBLE_EQ(130, result.x);
    EXPECT_DOUBLE_EQ(742, result.y);
    EXPECT_DOUBLE_EQ(std::numbers::pi / 2, result.theta);
  }
}

}  // namespace chrome_pdf
