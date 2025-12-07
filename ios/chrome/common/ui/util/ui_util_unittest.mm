// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/util/ui_util.h"

#import <UIKit/UIKit.h>
#import <stddef.h>

#import <array>

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using UIUtilTest = PlatformTest;

TEST_F(UIUtilTest, AlignToPixel) {
  CGFloat scale = [[UIScreen mainScreen] scale];
  // Pick a few interesting values: already aligned, aligned on retina, and
  // some unaligned values that would round differently. Ensure that all are
  // "integer" values within <1 of the original value in the scaled space.
  static constexpr auto kTestValues = std::to_array<CGPoint>({
      {10.0, 55.5},
      {55.5, 3.14159},
      {3.14159, 2.71828},
      {2.71828, 10.0},
  });

  const CGFloat kMaxAlignDelta = 0.9999;
  for (const CGPoint point : kTestValues) {
    const CGFloat alignedX = AlignValueToPixel(point.x);
    EXPECT_FLOAT_EQ(alignedX * scale, floor(alignedX * scale));
    EXPECT_NEAR(alignedX * scale, point.x * scale, kMaxAlignDelta);

    const CGPoint alignedPoint = AlignPointToPixel(point);
    EXPECT_FLOAT_EQ(floor(alignedPoint.x * scale), alignedPoint.x * scale);
    EXPECT_FLOAT_EQ(floor(alignedPoint.y * scale), alignedPoint.y * scale);
    EXPECT_NEAR(point.x * scale, alignedPoint.x * scale, kMaxAlignDelta);
    EXPECT_NEAR(point.y * scale, alignedPoint.y * scale, kMaxAlignDelta);
  }
}

#define EXPECT_EQ_RECT(a, b) \
  EXPECT_NSEQ(NSStringFromCGRect(a), NSStringFromCGRect(b))
#define EXPECT_EQ_SIZE(a, b) \
  EXPECT_NSEQ(NSStringFromCGSize(a), NSStringFromCGSize(b))

TEST_F(UIUtilTest, TestMakeCenteredRectInFrame) {
  CGSize frameSize, rectSize;
  CGRect expectedRectPosition, rectPosition;

  // Frame dimensions 100x100, rectangle dimensions 50x50
  frameSize = CGSizeMake(100, 100);
  rectSize = CGSizeMake(50, 50);

  expectedRectPosition = CGRectMake(25, 25, 50, 50);
  rectPosition = CGRectMakeCenteredRectInFrame(frameSize, rectSize);

  EXPECT_EQ_RECT(expectedRectPosition, rectPosition);

  // Frame dimensions 100x200, rectangle dimensions 40x40
  frameSize = CGSizeMake(100, 200);
  rectSize = CGSizeMake(40, 40);

  expectedRectPosition = CGRectMake(30, 80, 40, 40);
  rectPosition = CGRectMakeCenteredRectInFrame(frameSize, rectSize);

  EXPECT_EQ_RECT(expectedRectPosition, rectPosition);

  // Frame dimensions 100x200, rectangle dimensions 50x100
  frameSize = CGSizeMake(100, 200);
  rectSize = CGSizeMake(50, 100);

  expectedRectPosition = CGRectMake(25, 50, 50, 100);
  rectPosition = CGRectMakeCenteredRectInFrame(frameSize, rectSize);

  EXPECT_EQ_RECT(expectedRectPosition, rectPosition);

  // Frame dimensions 100x100, rectangle dimensions 50x20
  frameSize = CGSizeMake(100, 100);
  rectSize = CGSizeMake(50, 20);

  expectedRectPosition = CGRectMake(25, 40, 50, 20);
  rectPosition = CGRectMakeCenteredRectInFrame(frameSize, rectSize);

  EXPECT_EQ_RECT(expectedRectPosition, rectPosition);

  // Frame dimensions 100x100, rectangle dimensions 0x0
  frameSize = CGSizeMake(100, 100);
  rectSize = CGSizeMake(0, 0);

  expectedRectPosition = CGRectMake(50, 50, 0, 0);
  rectPosition = CGRectMakeCenteredRectInFrame(frameSize, rectSize);

  EXPECT_EQ_RECT(expectedRectPosition, rectPosition);
}
