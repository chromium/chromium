// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/int_rect.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

TEST(IntRectTest, ToString) {
  IntRect empty_rect = IntRect();
  EXPECT_EQ("0,0 0x0", empty_rect.ToString());

  IntRect rect(1, 2, 3, 4);
  EXPECT_EQ("1,2 3x4", rect.ToString());
}

TEST(IntRectTest, InclusiveIntersect) {
  IntRect rect(11, 12, 0, 0);
  EXPECT_TRUE(rect.InclusiveIntersect(IntRect(11, 12, 13, 14)));
  EXPECT_EQ(IntRect(11, 12, 0, 0), rect);

  rect = IntRect(11, 12, 13, 14);
  EXPECT_TRUE(rect.InclusiveIntersect(IntRect(24, 8, 0, 7)));
  EXPECT_EQ(IntRect(24, 12, 0, 3), rect);

  rect = IntRect(11, 12, 13, 14);
  EXPECT_TRUE(rect.InclusiveIntersect(IntRect(9, 15, 4, 0)));
  EXPECT_EQ(IntRect(11, 15, 2, 0), rect);

  rect = IntRect(11, 12, 0, 14);
  EXPECT_FALSE(rect.InclusiveIntersect(IntRect(12, 13, 15, 16)));
  EXPECT_EQ(IntRect(), rect);
}

TEST(IntRectTest, MaximumCoveredRect) {
  // X aligned and intersect: unite.
  EXPECT_EQ(
      IntRect(10, 20, 30, 60),
      MaximumCoveredRect(IntRect(10, 20, 30, 40), IntRect(10, 30, 30, 50)));
  // X aligned and adjacent: unite.
  EXPECT_EQ(
      IntRect(10, 20, 30, 90),
      MaximumCoveredRect(IntRect(10, 20, 30, 40), IntRect(10, 60, 30, 50)));
  // X aligned and separate: choose the bigger one.
  EXPECT_EQ(
      IntRect(10, 61, 30, 50),
      MaximumCoveredRect(IntRect(10, 20, 30, 40), IntRect(10, 61, 30, 50)));
  // Y aligned and intersect: unite.
  EXPECT_EQ(
      IntRect(10, 20, 60, 40),
      MaximumCoveredRect(IntRect(10, 20, 30, 40), IntRect(30, 20, 40, 40)));
  // Y aligned and adjacent: unite.
  EXPECT_EQ(
      IntRect(10, 20, 70, 40),
      MaximumCoveredRect(IntRect(10, 20, 30, 40), IntRect(40, 20, 40, 40)));
  // Y aligned and separate: choose the bigger one.
  EXPECT_EQ(
      IntRect(41, 20, 40, 40),
      MaximumCoveredRect(IntRect(10, 20, 30, 40), IntRect(41, 20, 40, 40)));
  // Get the biggest expanded intersection.
  EXPECT_EQ(IntRect(0, 0, 9, 19),
            MaximumCoveredRect(IntRect(0, 0, 10, 10), IntRect(0, 9, 9, 10)));
  EXPECT_EQ(IntRect(0, 0, 19, 9),
            MaximumCoveredRect(IntRect(0, 0, 10, 10), IntRect(9, 0, 10, 9)));
  // Otherwise choose the bigger one.
  EXPECT_EQ(
      IntRect(20, 30, 40, 50),
      MaximumCoveredRect(IntRect(10, 20, 30, 40), IntRect(20, 30, 40, 50)));
  EXPECT_EQ(
      IntRect(10, 20, 40, 50),
      MaximumCoveredRect(IntRect(10, 20, 40, 50), IntRect(20, 30, 30, 40)));
  EXPECT_EQ(
      IntRect(10, 20, 40, 50),
      MaximumCoveredRect(IntRect(10, 20, 40, 50), IntRect(20, 30, 40, 50)));
}

}  // namespace blink
