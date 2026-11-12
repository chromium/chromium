// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/duo_simulator_util.h"

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

constexpr CGFloat kCoverDisplayShortDimension = 393.0;
constexpr CGFloat kCoverDisplayLongDimension = 852.0;
constexpr CGFloat kInnerDisplayShortDimension = 669.0;
constexpr CGFloat kInnerDisplayLongDimension = 951.0;

using DuoSimulatorUtilTest = PlatformTest;

// Tests that `IsDuoCoverDisplaySize` returns `false` for an empty size.
TEST_F(DuoSimulatorUtilTest, FalseWhenSizeIsEmpty) {
  EXPECT_FALSE(IsDuoCoverDisplaySize(CGSizeZero));
}

// Tests that `IsDuoCoverDisplaySize` returns `true` for the cover display
// dimensions in both portrait and landscape orientations.
TEST_F(DuoSimulatorUtilTest, TrueForCoverDisplay) {
  EXPECT_TRUE(IsDuoCoverDisplaySize(
      CGSizeMake(kCoverDisplayShortDimension, kCoverDisplayLongDimension)));
  EXPECT_TRUE(IsDuoCoverDisplaySize(
      CGSizeMake(kCoverDisplayLongDimension, kCoverDisplayShortDimension)));
}

// Tests that `IsDuoCoverDisplaySize` returns `false` for the inner display
// dimensions in both portrait and landscape orientations.
TEST_F(DuoSimulatorUtilTest, FalseForInnerDisplay) {
  EXPECT_FALSE(IsDuoCoverDisplaySize(
      CGSizeMake(kInnerDisplayLongDimension, kInnerDisplayShortDimension)));
  EXPECT_FALSE(IsDuoCoverDisplaySize(
      CGSizeMake(kInnerDisplayShortDimension, kInnerDisplayLongDimension)));
}

}  // namespace
