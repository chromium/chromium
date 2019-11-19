// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/test/test_utils.h"

#include "ppapi/cpp/point.h"
#include "ppapi/cpp/rect.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_pdf {

void ComparePoint(const pp::Point& expected_point,
                  const pp::Point& given_point) {
  EXPECT_EQ(expected_point.x(), given_point.x());
  EXPECT_EQ(expected_point.y(), given_point.y());
}

void CompareRect(const pp::Rect& expected_rect, const pp::Rect& given_rect) {
  EXPECT_EQ(expected_rect.x(), given_rect.x());
  EXPECT_EQ(expected_rect.y(), given_rect.y());
  EXPECT_EQ(expected_rect.width(), given_rect.width());
  EXPECT_EQ(expected_rect.height(), given_rect.height());
}

void CompareRect(const pp::FloatRect& expected_rect,
                 const pp::FloatRect& given_rect) {
  EXPECT_FLOAT_EQ(expected_rect.x(), given_rect.x());
  EXPECT_FLOAT_EQ(expected_rect.y(), given_rect.y());
  EXPECT_FLOAT_EQ(expected_rect.width(), given_rect.width());
  EXPECT_FLOAT_EQ(expected_rect.height(), given_rect.height());
}

void CompareSize(const pp::Size& expected_size, const pp::Size& given_size) {
  EXPECT_EQ(expected_size.width(), given_size.width());
  EXPECT_EQ(expected_size.height(), given_size.height());
}

}  // namespace chrome_pdf