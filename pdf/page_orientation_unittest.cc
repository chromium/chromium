// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/page_orientation.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_pdf {
namespace {

TEST(PageOrientationTest, RotateClockwise) {
  EXPECT_EQ(RotateClockwise(PageOrientation::kOriginal),
            PageOrientation::kClockwise90);
  EXPECT_EQ(RotateClockwise(PageOrientation::kClockwise90),
            PageOrientation::kClockwise180);
  EXPECT_EQ(RotateClockwise(PageOrientation::kClockwise180),
            PageOrientation::kClockwise270);
  EXPECT_EQ(RotateClockwise(PageOrientation::kClockwise270),
            PageOrientation::kOriginal);
  EXPECT_EQ(RotateClockwise(PageOrientation::kLast),
            PageOrientation::kOriginal);
}

TEST(PageOrientationTest, RotateCounterclockwise) {
  EXPECT_EQ(RotateCounterclockwise(PageOrientation::kOriginal),
            PageOrientation::kClockwise270);
  EXPECT_EQ(RotateCounterclockwise(PageOrientation::kClockwise90),
            PageOrientation::kOriginal);
  EXPECT_EQ(RotateCounterclockwise(PageOrientation::kClockwise180),
            PageOrientation::kClockwise90);
  EXPECT_EQ(RotateCounterclockwise(PageOrientation::kClockwise270),
            PageOrientation::kClockwise180);
  EXPECT_EQ(RotateCounterclockwise(PageOrientation::kLast),
            PageOrientation::kClockwise180);
}

}  // namespace
}  // namespace chrome_pdf
