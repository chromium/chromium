// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/util/ui_util.h"

#import <UIKit/UIKit.h>
#include <stddef.h>

#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using UIUtilTest = PlatformTest;

TEST_F(UIUtilTest, AlignToPixel) {
  CGFloat scale = [[UIScreen mainScreen] scale];
  // Pick a few interesting values: already aligned, aligned on retina, and
  // some unaligned values that would round differently. Ensure that all are
  // "integer" values within <1 of the original value in the scaled space.
  CGFloat test_values[] = {10.0, 55.5, 3.14159, 2.71828};
  const CGFloat kMaxAlignDelta = 0.9999;
  size_t value_count = base::size(test_values);
  for (unsigned int i = 0; i < value_count; ++i) {
    CGFloat aligned = AlignValueToPixel(test_values[i]);
    EXPECT_FLOAT_EQ(aligned * scale, floor(aligned * scale));
    EXPECT_NEAR(aligned * scale, test_values[i] * scale, kMaxAlignDelta);

    CGFloat x = test_values[i];
    CGFloat y = test_values[(i + 1) % value_count];
    CGPoint alignedPoint = AlignPointToPixel(CGPointMake(x, y));
    EXPECT_FLOAT_EQ(floor(alignedPoint.x * scale), alignedPoint.x * scale);
    EXPECT_FLOAT_EQ(floor(alignedPoint.y * scale), alignedPoint.y * scale);
    EXPECT_NEAR(x * scale, alignedPoint.x * scale, kMaxAlignDelta);
    EXPECT_NEAR(y * scale, alignedPoint.y * scale, kMaxAlignDelta);
  }
}

#define EXPECT_EQ_RECT(a, b) \
  EXPECT_NSEQ(NSStringFromCGRect(a), NSStringFromCGRect(b))
#define EXPECT_EQ_SIZE(a, b) \
  EXPECT_NSEQ(NSStringFromCGSize(a), NSStringFromCGSize(b))

TEST_F(UIUtilTest, TestProjectionFill) {
  CGSize originalSize, targetSize, expectedRevisedSize, revisedSize;
  CGRect expectedProjection, projection;

  // Resize with same aspect ratio.
  originalSize = CGSizeMake(100, 100);
  targetSize = CGSizeMake(50, 50);
  expectedRevisedSize = targetSize;
  expectedProjection = CGRectMake(0, 0, 50, 50);
  CalculateProjection(originalSize, targetSize, ProjectionMode::kFill,
                      revisedSize, projection);
  EXPECT_EQ_RECT(expectedProjection, projection);
  EXPECT_EQ_SIZE(expectedRevisedSize, revisedSize);

  // Resize with different aspect ratio.
  originalSize = CGSizeMake(100, 100);
  targetSize = CGSizeMake(60, 40);
  expectedRevisedSize = targetSize;
  expectedProjection = CGRectMake(0, 0, 60, 40);
  CalculateProjection(originalSize, targetSize, ProjectionMode::kFill,
                      revisedSize, projection);
  EXPECT_EQ_RECT(expectedProjection, projection);
  EXPECT_EQ_SIZE(expectedRevisedSize, revisedSize);

  // Stretch the other way.
  originalSize = CGSizeMake(100, 100);
  targetSize = CGSizeMake(40, 60);
  expectedRevisedSize = targetSize;
  expectedProjection = CGRectMake(0, 0, 40, 60);
  CalculateProjection(originalSize, targetSize, ProjectionMode::kFill,
                      revisedSize, projection);
  EXPECT_EQ_RECT(expectedProjection, projection);
  EXPECT_EQ_SIZE(expectedRevisedSize, revisedSize);
}

TEST_F(UIUtilTest, TestProjectionFit) {
  CGSize originalSize, targetSize, expectedRevisedSize, revisedSize;
  CGRect expectedProjection, projection;

  // Landscape resize to 50x50, but squeezed into 50x25.
  originalSize = CGSizeMake(100, 50);
  targetSize = CGSizeMake(50, 50);
  expectedRevisedSize = CGSizeMake(50, 25);
  expectedProjection = CGRectMake(0, 0, 50, 25);
  CalculateProjection(originalSize, targetSize, ProjectionMode::kAspectFit,
                      revisedSize, projection);
  EXPECT_EQ_RECT(expectedProjection, projection);
  EXPECT_EQ_SIZE(expectedRevisedSize, revisedSize);

  // Landscape resize to 60x40, but squeezed into 60x30.
  originalSize = CGSizeMake(100, 50);
  targetSize = CGSizeMake(60, 40);
  expectedRevisedSize = CGSizeMake(60, 30);
  expectedProjection = CGRectMake(0, 0, 60, 30);
  CalculateProjection(originalSize, targetSize, ProjectionMode::kAspectFit,
                      revisedSize, projection);
  EXPECT_EQ_RECT(expectedProjection, projection);
  EXPECT_EQ_SIZE(expectedRevisedSize, revisedSize);

  // Landscape resize to 40x60, but squeezed into 40x20.
  originalSize = CGSizeMake(100, 50);
  targetSize = CGSizeMake(40, 60);
  expectedRevisedSize = CGSizeMake(40, 20);
  expectedProjection = CGRectMake(0, 0, 40, 20);
  CalculateProjection(originalSize, targetSize, ProjectionMode::kAspectFit,
                      revisedSize, projection);
  EXPECT_EQ_RECT(expectedProjection, projection);
  EXPECT_EQ_SIZE(expectedRevisedSize, revisedSize);

  // Portrait resize to 50x50, but squeezed into 25x50.
  originalSize = CGSizeMake(50, 100);
  targetSize = CGSizeMake(50, 50);
  expectedRevisedSize = CGSizeMake(25, 50);
  expectedProjection = CGRectMake(0, 0, 25, 50);
  CalculateProjection(originalSize, targetSize, ProjectionMode::kAspectFit,
                      revisedSize, projection);
  EXPECT_EQ_RECT(expectedProjection, projection);
  EXPECT_EQ_SIZE(expectedRevisedSize, revisedSize);

  // Portrait resize to 60x40, but squeezed into 20x40.
  originalSize = CGSizeMake(50, 100);
  targetSize = CGSizeMake(60, 40);
  expectedRevisedSize = CGSizeMake(20, 40);
  expectedProjection = CGRectMake(0, 0, 20, 40);
  CalculateProjection(originalSize, targetSize, ProjectionMode::kAspectFit,
                      revisedSize, projection);
  EXPECT_EQ_RECT(expectedProjection, projection);
  EXPECT_EQ_SIZE(expectedRevisedSize, revisedSize);

  // Portrait resize to 40x60, but squeezed into 30x60.
  originalSize = CGSizeMake(50, 100);
  targetSize = CGSizeMake(40, 60);
  expectedRevisedSize = CGSizeMake(30, 60);
  expectedProjection = CGRectMake(0, 0, 30, 60);
  CalculateProjection(originalSize, targetSize, ProjectionMode::kAspectFit,
                      revisedSize, projection);
  EXPECT_EQ_RECT(expectedProjection, projection);
  EXPECT_EQ_SIZE(expectedRevisedSize, revisedSize);
}

TEST_F(UIUtilTest, TestProjectionAspectFill) {
  CGSize originalSize, targetSize, expectedRevisedSize, revisedSize;
  CGRect expectedProjection, projection;

  // Landscape resize to 50x50
  originalSize = CGSizeMake(100, 50);
  targetSize = CGSizeMake(50, 50);
  expectedRevisedSize = targetSize;
  expectedProjection = CGRectMake(-25, 0, 100, 50);
  CalculateProjection(originalSize, targetSize, ProjectionMode::kAspectFill,
                      revisedSize, projection);
  EXPECT_EQ_RECT(expectedProjection, projection);
  EXPECT_EQ_SIZE(expectedRevisedSize, revisedSize);

  // Landscape resize to 60x40, but squeezed into 60x30.
  originalSize = CGSizeMake(100, 50);
  targetSize = CGSizeMake(60, 40);
  expectedRevisedSize = targetSize;
  expectedProjection = CGRectMake(-10, 0, 80, 40);
  CalculateProjection(originalSize, targetSize, ProjectionMode::kAspectFill,
                      revisedSize, projection);
  EXPECT_EQ_RECT(expectedProjection, projection);
  EXPECT_EQ_SIZE(expectedRevisedSize, revisedSize);

  // Landscape resize to 40x60, but squeezed into 40x20.
  originalSize = CGSizeMake(100, 50);
  targetSize = CGSizeMake(40, 60);
  expectedRevisedSize = targetSize;
  expectedProjection = CGRectMake(-40, 0, 120, 60);
  CalculateProjection(originalSize, targetSize, ProjectionMode::kAspectFill,
                      revisedSize, projection);
  EXPECT_EQ_RECT(expectedProjection, projection);
  EXPECT_EQ_SIZE(expectedRevisedSize, revisedSize);

  // Portrait resize to 50x50, but squeezed into 25x50.
  originalSize = CGSizeMake(50, 100);
  targetSize = CGSizeMake(50, 50);
  expectedRevisedSize = targetSize;
  expectedProjection = CGRectMake(0, -25, 50, 100);
  CalculateProjection(originalSize, targetSize, ProjectionMode::kAspectFill,
                      revisedSize, projection);
  EXPECT_EQ_RECT(expectedProjection, projection);
  EXPECT_EQ_SIZE(expectedRevisedSize, revisedSize);

  // Portrait resize to 60x40, but clipped to 20x40.
  originalSize = CGSizeMake(50, 100);
  targetSize = CGSizeMake(60, 40);
  expectedRevisedSize = targetSize;
  expectedProjection = CGRectMake(0, -40, 60, 120);
  CalculateProjection(originalSize, targetSize, ProjectionMode::kAspectFill,
                      revisedSize, projection);
  EXPECT_EQ_RECT(expectedProjection, projection);
  EXPECT_EQ_SIZE(expectedRevisedSize, revisedSize);

  // Portrait resize to 40x60, but clipped to 30x60.
  originalSize = CGSizeMake(50, 100);
  targetSize = CGSizeMake(40, 60);
  expectedRevisedSize = targetSize;
  expectedProjection = CGRectMake(0, -10, 40, 80);
  CalculateProjection(originalSize, targetSize, ProjectionMode::kAspectFill,
                      revisedSize, projection);
  EXPECT_EQ_RECT(expectedProjection, projection);
  EXPECT_EQ_SIZE(expectedRevisedSize, revisedSize);
}

TEST_F(UIUtilTest, TestProjectionAspectFillAlignTop) {
  CGSize originalSize, targetSize, expectedRevisedSize, revisedSize;
  CGRect expectedProjection, projection;

  // Landscape resize to 100x100
  originalSize = CGSizeMake(400, 200);
  targetSize = CGSizeMake(100, 100);
  expectedRevisedSize = targetSize;
  expectedProjection = CGRectMake(-50, 0, 200, 100);
  CalculateProjection(originalSize, targetSize,
                      ProjectionMode::kAspectFillAlignTop, revisedSize,
                      projection);
  EXPECT_EQ_RECT(expectedProjection, projection);
  EXPECT_EQ_SIZE(expectedRevisedSize, revisedSize);

  // Portrait resize to 100x100 and aligned to top
  originalSize = CGSizeMake(200, 400);
  targetSize = CGSizeMake(100, 100);
  expectedRevisedSize = targetSize;
  expectedProjection = CGRectMake(0, 0, 100, 200);
  CalculateProjection(originalSize, targetSize,
                      ProjectionMode::kAspectFillAlignTop, revisedSize,
                      projection);
  EXPECT_EQ_RECT(expectedProjection, projection);
  EXPECT_EQ_SIZE(expectedRevisedSize, revisedSize);
}

TEST_F(UIUtilTest, TestProjectionAspectFillNoClipping) {
  CGSize originalSize, targetSize, expectedRevisedSize, revisedSize;
  CGRect expectedProjection, projection;

  // Landscape resize to 50x50
  originalSize = CGSizeMake(100, 50);
  targetSize = CGSizeMake(50, 50);
  expectedProjection = CGRectMake(0, 0, 100, 50);
  expectedRevisedSize = expectedProjection.size;
  CalculateProjection(originalSize, targetSize,
                      ProjectionMode::kAspectFillNoClipping, revisedSize,
                      projection);
  EXPECT_EQ_RECT(expectedProjection, projection);
  EXPECT_EQ_SIZE(expectedRevisedSize, revisedSize);

  // Landscape resize to 60x40.
  originalSize = CGSizeMake(100, 50);
  targetSize = CGSizeMake(60, 40);
  expectedProjection = CGRectMake(0, 0, 80, 40);
  expectedRevisedSize = expectedProjection.size;
  CalculateProjection(originalSize, targetSize,
                      ProjectionMode::kAspectFillNoClipping, revisedSize,
                      projection);
  EXPECT_EQ_RECT(expectedProjection, projection);
  EXPECT_EQ_SIZE(expectedRevisedSize, revisedSize);

  // Landscape resize to 40x60.
  originalSize = CGSizeMake(100, 50);
  targetSize = CGSizeMake(40, 60);
  expectedProjection = CGRectMake(0, 0, 120, 60);
  expectedRevisedSize = expectedProjection.size;
  CalculateProjection(originalSize, targetSize,
                      ProjectionMode::kAspectFillNoClipping, revisedSize,
                      projection);
  EXPECT_EQ_RECT(expectedProjection, projection);
  EXPECT_EQ_SIZE(expectedRevisedSize, revisedSize);

  // Portrait resize to 50x50.
  originalSize = CGSizeMake(50, 100);
  targetSize = CGSizeMake(50, 50);
  expectedProjection = CGRectMake(0, 0, 50, 100);
  expectedRevisedSize = expectedProjection.size;
  CalculateProjection(originalSize, targetSize,
                      ProjectionMode::kAspectFillNoClipping, revisedSize,
                      projection);
  EXPECT_EQ_RECT(expectedProjection, projection);
  EXPECT_EQ_SIZE(expectedRevisedSize, revisedSize);

  // Portrait resize to 60x40.
  originalSize = CGSizeMake(50, 100);
  targetSize = CGSizeMake(60, 40);
  expectedProjection = CGRectMake(0, 0, 60, 120);
  expectedRevisedSize = expectedProjection.size;
  CalculateProjection(originalSize, targetSize,
                      ProjectionMode::kAspectFillNoClipping, revisedSize,
                      projection);
  EXPECT_EQ_RECT(expectedProjection, projection);
  EXPECT_EQ_SIZE(expectedRevisedSize, revisedSize);

  // Portrait resize to 40x60.
  originalSize = CGSizeMake(50, 100);
  targetSize = CGSizeMake(40, 60);
  expectedProjection = CGRectMake(0, 0, 40, 80);
  expectedRevisedSize = expectedProjection.size;
  CalculateProjection(originalSize, targetSize,
                      ProjectionMode::kAspectFillNoClipping, revisedSize,
                      projection);
  EXPECT_EQ_RECT(expectedProjection, projection);
  EXPECT_EQ_SIZE(expectedRevisedSize, revisedSize);
}

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
