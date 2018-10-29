// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar_container/toolbar_height_range.h"

#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using toolbar_container::HeightRange;

// Test fixture for HeightRange.
using HeightRangeTest = PlatformTest;

// Tests that the default constructor creates a [0.0, 0.0] range.
TEST_F(HeightRangeTest, DefaultConstructor) {
  HeightRange range;
  EXPECT_EQ(range.min_height(), 0.0);
  EXPECT_EQ(range.max_height(), 0.0);
}

// Simple test for setting the height range values.
TEST_F(HeightRangeTest, Creation) {
  const CGFloat kMin = 50.0;
  const CGFloat kMax = 100.0;
  HeightRange range(kMin, kMax);
  EXPECT_EQ(range.min_height(), kMin);
  EXPECT_EQ(range.max_height(), kMax);
  EXPECT_EQ(range.delta(), kMax - kMin);
}

// Test for getting interpolation values.
TEST_F(HeightRangeTest, Interpolation) {
  const CGFloat kMin = 0.0;
  const CGFloat kMax = 100.0;
  HeightRange range(kMin, kMax);
  EXPECT_EQ(range.GetInterpolatedHeight(-0.5), 0.0);
  EXPECT_EQ(range.GetInterpolatedHeight(0.0), 0.0);
  EXPECT_EQ(range.GetInterpolatedHeight(0.25), 25.0);
  EXPECT_EQ(range.GetInterpolatedHeight(0.5), 50.0);
  EXPECT_EQ(range.GetInterpolatedHeight(0.75), 75.0);
  EXPECT_EQ(range.GetInterpolatedHeight(1.0), 100.0);
  EXPECT_EQ(range.GetInterpolatedHeight(1.5), 100.0);
}

// Test for comparing ranges.
TEST_F(HeightRangeTest, Equality) {
  const CGFloat kMin = 0.0;
  const CGFloat kMax = 100.0;
  HeightRange range(kMin, kMax);
  HeightRange equal_range(kMin, kMax);
  EXPECT_EQ(range, equal_range);
  HeightRange unequal_range;
  EXPECT_NE(range, unequal_range);
}

// Test for the + and - operators
TEST_F(HeightRangeTest, AdditionSubtraction) {
  const CGFloat kMin1 = 0.0;
  const CGFloat kMax1 = 100.0;
  HeightRange range1(kMin1, kMax1);
  const CGFloat kMin2 = 80.0;
  const CGFloat kMax2 = 110.0;
  HeightRange range2(kMin2, kMax2);
  HeightRange sum = range1 + range2;
  EXPECT_EQ(sum, HeightRange(kMin1 + kMin2, kMax1 + kMax2));
  EXPECT_EQ(sum - range2, range1);
}

// Test for the += and -= operators.
TEST_F(HeightRangeTest, AssignAdditionSubtraction) {
  const CGFloat kMin = 0.0;
  const CGFloat kMax = 100.0;
  HeightRange range(kMin, kMax);
  range += range;
  EXPECT_EQ(range, HeightRange(2.0 * kMin, 2.0 * kMax));
  range -= HeightRange(kMin, kMax);
  EXPECT_EQ(range, HeightRange(kMin, kMax));
}
