// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bubble/bubble_util.h"

#include <CoreGraphics/CoreGraphics.h>

#include "ios/chrome/browser/ui/util/ui_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
CGFloat TestBubbleAlignmentOffset() {
  return 29;
}
}  // namespace

namespace bubble_util {
CGSize BubbleMaxSize(CGPoint anchorPoint,
                     BubbleArrowDirection direction,
                     BubbleAlignment alignment,
                     CGSize boundingSize,
                     bool isRTL);

CGRect BubbleFrame(CGPoint anchorPoint,
                   CGSize size,
                   BubbleArrowDirection direction,
                   BubbleAlignment alignment,
                   CGFloat boundingWidth,
                   bool isRTL);
}  // namespace bubble_util

class BubbleUtilTest : public PlatformTest {
 public:
  BubbleUtilTest()
      : leftAlignedAnchorPoint_({70.0f, 250.0f}),
        centerAlignedAnchorPoint_({300.0f, 250.0f}),
        rightAlignedAnchorPoint_({450.0f, 250.0f}),
        bubbleSize_({300.0f, 100.0f}),
        containerSize_({500.0f, 600.0f}) {}

 protected:
  // Anchor point on the left side of the container.
  const CGPoint leftAlignedAnchorPoint_;
  // Anchor point on the center of the container.
  const CGPoint centerAlignedAnchorPoint_;
  // Anchor point on the right side of the container.
  const CGPoint rightAlignedAnchorPoint_;
  // Size of the bubble.
  const CGSize bubbleSize_;
  // Bounding size of the bubble's coordinate system.
  const CGSize containerSize_;
};

// Test the |AnchorPoint| method when the arrow is pointing upwards, meaning the
// bubble is below the UI element.
TEST_F(BubbleUtilTest, AnchorPointUp) {
  CGPoint anchorPoint = bubble_util::AnchorPoint(
      {{250.0f, 200.0f}, {100.0f, 100.0f}}, BubbleArrowDirectionUp);
  EXPECT_TRUE(CGPointEqualToPoint({300.0f, 300.0f}, anchorPoint));
}

// Test the |AnchorPoint| method when the arrow is pointing downwards, meaning
// the bubble is above the UI element.
TEST_F(BubbleUtilTest, AnchorPointDown) {
  CGPoint anchorPoint = bubble_util::AnchorPoint(
      {{250.0f, 200.0f}, {100.0f, 100.0f}}, BubbleArrowDirectionDown);
  EXPECT_TRUE(CGPointEqualToPoint({300.0f, 200.0f}, anchorPoint));
}

// Test the |BubbleMaxSize| method when the bubble is leading aligned, the
// target is on the left side of the container, the bubble is pointing up, and
// the language is LTR.
TEST_F(BubbleUtilTest, BubbleMaxSizeLeadingWithTargetOnLeftUp) {
  CGSize leftAlignedSize = bubble_util::BubbleMaxSize(
      leftAlignedAnchorPoint_, BubbleArrowDirectionUp, BubbleAlignmentLeading,
      containerSize_, false /* isRTL */);

  EXPECT_FLOAT_EQ(430.0f + TestBubbleAlignmentOffset(), leftAlignedSize.width);
  EXPECT_FLOAT_EQ(350.0f, leftAlignedSize.height);
}

// Test the |BubbleMaxSize| method when the bubble is leading aligned, the
// target is on the center of the container, the bubble is pointing down, and
// the language is LTR.
TEST_F(BubbleUtilTest, BubbleMaxSizeLeadingWithTargetOnCenterDown) {
  CGSize centerAlignedSize = bubble_util::BubbleMaxSize(
      centerAlignedAnchorPoint_, BubbleArrowDirectionDown,
      BubbleAlignmentLeading, containerSize_, false /* isRTL */);

  EXPECT_FLOAT_EQ(200.0f + TestBubbleAlignmentOffset(),
                  centerAlignedSize.width);
  EXPECT_FLOAT_EQ(250.0f, centerAlignedSize.height);
}

// Test the |BubbleMaxSize| method when the bubble is leading aligned, the
// target is on the right side of the container, the bubble is pointing up, and
// the language is LTR.
TEST_F(BubbleUtilTest, BubbleMaxSizeLeadingWithTargetOnRightUp) {
  CGSize rightAlignedSize = bubble_util::BubbleMaxSize(
      rightAlignedAnchorPoint_, BubbleArrowDirectionUp, BubbleAlignmentLeading,
      containerSize_, false /* isRTL */);

  EXPECT_FLOAT_EQ(50.0f + TestBubbleAlignmentOffset(), rightAlignedSize.width);
  EXPECT_FLOAT_EQ(350.0f, rightAlignedSize.height);
}

// Test the |BubbleMaxSize| method when the bubble is center aligned, the target
// is on the left side of the container, the bubble is pointing down, and the
// language is LTR.
TEST_F(BubbleUtilTest, BubbleMaxSizeCenterWithTargetOnLeftDown) {
  CGSize leftAlignedSize = bubble_util::BubbleMaxSize(
      leftAlignedAnchorPoint_, BubbleArrowDirectionDown, BubbleAlignmentCenter,
      containerSize_, false /* isRTL */);

  EXPECT_FLOAT_EQ(140.0f, leftAlignedSize.width);
  EXPECT_FLOAT_EQ(250.0f, leftAlignedSize.height);
}

// Test the |BubbleMaxSize| method when the bubble is center aligned, the target
// is on the center of the container, the bubble is pointing up, and the
// language is LTR.
TEST_F(BubbleUtilTest, BubbleMaxSizeCenterWithTargetOnCenterUp) {
  CGSize centerAlignedSize = bubble_util::BubbleMaxSize(
      centerAlignedAnchorPoint_, BubbleArrowDirectionUp, BubbleAlignmentCenter,
      containerSize_, false /* isRTL */);

  EXPECT_FLOAT_EQ(400.0f, centerAlignedSize.width);
  EXPECT_FLOAT_EQ(350.0f, centerAlignedSize.height);
}

// Test the |BubbleMaxSize| method when the bubble is center aligned, the target
// is on the right side of the container, the bubble is pointing down, and the
// language is LTR.
TEST_F(BubbleUtilTest, BubbleMaxSizeCenterWithTargetOnRightDown) {
  CGSize rightAlignedSize = bubble_util::BubbleMaxSize(
      rightAlignedAnchorPoint_, BubbleArrowDirectionDown, BubbleAlignmentCenter,
      containerSize_, false /* isRTL */);

  EXPECT_FLOAT_EQ(100.0f, rightAlignedSize.width);
  EXPECT_FLOAT_EQ(250.0f, rightAlignedSize.height);
}

// Test the |BubbleMaxSize| method when the bubble is trailing aligned, the
// target is on the left side of the container, the bubble is pointing up, and
// the language is LTR.
TEST_F(BubbleUtilTest, BubbleMaxSizeTrailingWithTargetOnLeftUp) {
  CGSize leftAlignedSize = bubble_util::BubbleMaxSize(
      leftAlignedAnchorPoint_, BubbleArrowDirectionUp, BubbleAlignmentTrailing,
      containerSize_, false /* isRTL */);

  EXPECT_FLOAT_EQ(70.0f + TestBubbleAlignmentOffset(), leftAlignedSize.width);
  EXPECT_FLOAT_EQ(350.0f, leftAlignedSize.height);
}

// Test the |BubbleMaxSize| method when the bubble is trailing aligned, the
// target is on the center of the container, the bubble is pointing down, and
// the language is LTR.
TEST_F(BubbleUtilTest, BubbleMaxSizeTrailingWithTargetOnCenterDown) {
  CGSize centerAlignedSize = bubble_util::BubbleMaxSize(
      centerAlignedAnchorPoint_, BubbleArrowDirectionDown,
      BubbleAlignmentTrailing, containerSize_, false /* isRTL */);

  EXPECT_FLOAT_EQ(300.0f + TestBubbleAlignmentOffset(),
                  centerAlignedSize.width);
  EXPECT_FLOAT_EQ(250.0f, centerAlignedSize.height);
}

// Test the |BubbleMaxSize| method when the bubble is trailing aligned, the
// target is on the right side of the container, the bubble is pointing up, and
// the language is LTR.
TEST_F(BubbleUtilTest, BubbleMaxSizeTrailingWithTargetOnRightUp) {
  CGSize rightAlignedSize = bubble_util::BubbleMaxSize(
      rightAlignedAnchorPoint_, BubbleArrowDirectionUp, BubbleAlignmentTrailing,
      containerSize_, false /* isRTL */);

  EXPECT_FLOAT_EQ(450.0f + TestBubbleAlignmentOffset(), rightAlignedSize.width);
  EXPECT_FLOAT_EQ(350.0f, rightAlignedSize.height);
}

// Test the |BubbleMaxSize| method when the bubble is leading aligned, the
// target is on the left side of the container, the bubble is pointing down, and
// the language is RTL.
TEST_F(BubbleUtilTest, BubbleMaxSizeLeadingWithTargetOnLeftDownRTL) {
  CGSize leftAlignedSizeRTL = bubble_util::BubbleMaxSize(
      leftAlignedAnchorPoint_, BubbleArrowDirectionDown, BubbleAlignmentLeading,
      containerSize_, true /* isRTL */);

  EXPECT_FLOAT_EQ(70.0f + TestBubbleAlignmentOffset(),
                  leftAlignedSizeRTL.width);
  EXPECT_FLOAT_EQ(250.0f, leftAlignedSizeRTL.height);
}

// Test the |BubbleMaxSize| method when the bubble is leading aligned, the
// target is on the center of the container, the bubble is pointing up, and the
// language is RTL.
TEST_F(BubbleUtilTest, BubbleMaxSizeLeadingWithTargetOnCenterUpRTL) {
  CGSize centerAlignedSizeRTL = bubble_util::BubbleMaxSize(
      centerAlignedAnchorPoint_, BubbleArrowDirectionUp, BubbleAlignmentLeading,
      containerSize_, true /* isRTL */);

  EXPECT_FLOAT_EQ(300.0f + TestBubbleAlignmentOffset(),
                  centerAlignedSizeRTL.width);
  EXPECT_FLOAT_EQ(350.0f, centerAlignedSizeRTL.height);
}

// Test the |BubbleMaxSize| method when the bubble is leading aligned, the
// target is on the right side of the container, the bubble is pointing down,
// and the language is RTL.
TEST_F(BubbleUtilTest, BubbleMaxSizeLeadingWithTargetOnRightDownRTL) {
  CGSize rightAlignedSizeRTL = bubble_util::BubbleMaxSize(
      rightAlignedAnchorPoint_, BubbleArrowDirectionDown,
      BubbleAlignmentLeading, containerSize_, true /* isRTL */);

  EXPECT_FLOAT_EQ(450.0f + TestBubbleAlignmentOffset(),
                  rightAlignedSizeRTL.width);
  EXPECT_FLOAT_EQ(250.0f, rightAlignedSizeRTL.height);
}

// Test the |BubbleMaxSize| method when the bubble is center aligned, the target
// is on the left side of the container, the bubble is pointing up, and the
// language is RTL.
TEST_F(BubbleUtilTest, BubbleMaxSizeCenterWithTargetOnLeftUpRTL) {
  CGSize leftAlignedSizeRTL = bubble_util::BubbleMaxSize(
      leftAlignedAnchorPoint_, BubbleArrowDirectionUp, BubbleAlignmentCenter,
      containerSize_, true /* isRTL */);

  EXPECT_FLOAT_EQ(140.0f, leftAlignedSizeRTL.width);
  EXPECT_FLOAT_EQ(350.0f, leftAlignedSizeRTL.height);
}

// Test the |BubbleMaxSize| method when the bubble is center aligned, the target
// is on the center of the container, the bubble is pointing down, and the
// language is RTL.
TEST_F(BubbleUtilTest, BubbleMaxSizeCenterWithTargetOnCenterDownRTL) {
  CGSize centerAlignedSizeRTL = bubble_util::BubbleMaxSize(
      centerAlignedAnchorPoint_, BubbleArrowDirectionDown,
      BubbleAlignmentCenter, containerSize_, true /* isRTL */);

  EXPECT_FLOAT_EQ(400.0f, centerAlignedSizeRTL.width);
  EXPECT_FLOAT_EQ(250.0f, centerAlignedSizeRTL.height);
}

// Test the |BubbleMaxSize| method when the bubble is center aligned, the target
// is on the right side of the container, the bubble is pointing up, and the
// language is RTL.
TEST_F(BubbleUtilTest, BubbleMaxSizeCenterWithTargetOnRightUpRTL) {
  CGSize rightAlignedSizeRTL = bubble_util::BubbleMaxSize(
      rightAlignedAnchorPoint_, BubbleArrowDirectionUp, BubbleAlignmentCenter,
      containerSize_, true /* isRTL */);

  EXPECT_FLOAT_EQ(100.0f, rightAlignedSizeRTL.width);
  EXPECT_FLOAT_EQ(350.0f, rightAlignedSizeRTL.height);
}

// Test the |BubbleMaxSize| method when the bubble is trailing aligned, the
// target is on the left side of the container, the bubble is pointing down, and
// the language is RTL.
TEST_F(BubbleUtilTest, BubbleMaxSizeTrailingWithTargetOnLeftDownRTL) {
  CGSize leftAlignedSizeRTL = bubble_util::BubbleMaxSize(
      leftAlignedAnchorPoint_, BubbleArrowDirectionDown,
      BubbleAlignmentTrailing, containerSize_, true /* isRTL */);

  EXPECT_FLOAT_EQ(430.0f + TestBubbleAlignmentOffset(),
                  leftAlignedSizeRTL.width);
  EXPECT_FLOAT_EQ(250.0f, leftAlignedSizeRTL.height);
}

// Test the |BubbleMaxSize| method when the bubble is trailing aligned, the
// target is on the center of the container, the bubble is pointing up, and the
// language is RTL.
TEST_F(BubbleUtilTest, BubbleMaxSizeTrailingWithTargetOnCenterUpRTL) {
  CGSize centerAlignedSizeRTL = bubble_util::BubbleMaxSize(
      centerAlignedAnchorPoint_, BubbleArrowDirectionUp,
      BubbleAlignmentTrailing, containerSize_, true /* isRTL */);

  EXPECT_FLOAT_EQ(200.0f + TestBubbleAlignmentOffset(),
                  centerAlignedSizeRTL.width);
  EXPECT_FLOAT_EQ(350.0f, centerAlignedSizeRTL.height);
}

// Test the |BubbleMaxSize| method when the bubble is trailing aligned, the
// target is on the right side of the container, the bubble is pointing down,
// and the language is RTL.
TEST_F(BubbleUtilTest, BubbleMaxSizeTrailingWithTargetOnRightDownRTL) {
  CGSize rightAlignedSizeRTL = bubble_util::BubbleMaxSize(
      rightAlignedAnchorPoint_, BubbleArrowDirectionDown,
      BubbleAlignmentTrailing, containerSize_, true /* isRTL */);

  EXPECT_FLOAT_EQ(50.0f + TestBubbleAlignmentOffset(),
                  rightAlignedSizeRTL.width);
  EXPECT_FLOAT_EQ(250.0f, rightAlignedSizeRTL.height);
}

// Test |BubbleFrame| when the bubble's direction is
// |BubbleArrowDirectionUp|, the alignment is |BubbleAlignmentLeading|, and the
// language is LTR.
TEST_F(BubbleUtilTest, BubbleFrameUpLeadingLTR) {
  CGRect bubbleFrame = bubble_util::BubbleFrame(
      centerAlignedAnchorPoint_, bubbleSize_, BubbleArrowDirectionUp,
      BubbleAlignmentLeading, containerSize_.width, false /* isRTL */);
  EXPECT_FLOAT_EQ(300.0f - TestBubbleAlignmentOffset(), bubbleFrame.origin.x);
  EXPECT_FLOAT_EQ(250.0f, bubbleFrame.origin.y);
  EXPECT_FLOAT_EQ(bubbleSize_.width, CGRectGetWidth(bubbleFrame));
  EXPECT_FLOAT_EQ(bubbleSize_.height, CGRectGetHeight(bubbleFrame));
}

// Test |BubbleFrame| when the bubble's direction is
// |BubbleArrowDirectionUp|, the alignment is |BubbleAlignmentLeading|, and the
// language is RTL.
TEST_F(BubbleUtilTest, BubbleFrameUpLeadingRTL) {
  CGRect bubbleFrame = bubble_util::BubbleFrame(
      centerAlignedAnchorPoint_, bubbleSize_, BubbleArrowDirectionUp,
      BubbleAlignmentLeading, containerSize_.width, true /* isRTL */);
  EXPECT_FLOAT_EQ(TestBubbleAlignmentOffset(), bubbleFrame.origin.x);
  EXPECT_FLOAT_EQ(250.0f, bubbleFrame.origin.y);
  EXPECT_FLOAT_EQ(bubbleSize_.width, CGRectGetWidth(bubbleFrame));
  EXPECT_FLOAT_EQ(bubbleSize_.height, CGRectGetHeight(bubbleFrame));
}

// Test |BubbleFrame| when the bubble's direction is
// |BubbleArrowDirectionUp|, the alignment is |BubbleAlignmentCenter|, and the
// language is LTR.
TEST_F(BubbleUtilTest, BubbleFrameUpCenteredLTR) {
  CGRect bubbleFrame = bubble_util::BubbleFrame(
      centerAlignedAnchorPoint_, bubbleSize_, BubbleArrowDirectionUp,
      BubbleAlignmentCenter, containerSize_.width, false /* isRTL */);
  EXPECT_FLOAT_EQ(150.0f, bubbleFrame.origin.x);
  EXPECT_FLOAT_EQ(250.0f, bubbleFrame.origin.y);
  EXPECT_FLOAT_EQ(bubbleSize_.width, CGRectGetWidth(bubbleFrame));
  EXPECT_FLOAT_EQ(bubbleSize_.height, CGRectGetHeight(bubbleFrame));
}

// Test |BubbleFrame| when the bubble's direction is
// |BubbleArrowDirectionUp|, the alignment is |BubbleAlignmentCenter|, and the
// language is RTL.
TEST_F(BubbleUtilTest, BubbleFrameUpCenteredRTL) {
  CGRect bubbleFrame = bubble_util::BubbleFrame(
      centerAlignedAnchorPoint_, bubbleSize_, BubbleArrowDirectionUp,
      BubbleAlignmentCenter, containerSize_.width, true /* isRTL */);
  EXPECT_FLOAT_EQ(150.0f, bubbleFrame.origin.x);
  EXPECT_FLOAT_EQ(250.0f, bubbleFrame.origin.y);
  EXPECT_FLOAT_EQ(bubbleSize_.width, CGRectGetWidth(bubbleFrame));
  EXPECT_FLOAT_EQ(bubbleSize_.height, CGRectGetHeight(bubbleFrame));
}

// Test |BubbleFrame| when the bubble's direction is
// |BubbleArrowDirectionUp|, the alignment is |BubbleAlignmentTrailing|, and the
// language is LTR.
TEST_F(BubbleUtilTest, BubbleFrameUpTrailingLTR) {
  CGRect bubbleFrame = bubble_util::BubbleFrame(
      centerAlignedAnchorPoint_, bubbleSize_, BubbleArrowDirectionUp,
      BubbleAlignmentTrailing, containerSize_.width, false /* isRTL */);
  EXPECT_FLOAT_EQ(TestBubbleAlignmentOffset(), bubbleFrame.origin.x);
  EXPECT_FLOAT_EQ(250.0f, bubbleFrame.origin.y);
  EXPECT_FLOAT_EQ(bubbleSize_.width, CGRectGetWidth(bubbleFrame));
  EXPECT_FLOAT_EQ(bubbleSize_.height, CGRectGetHeight(bubbleFrame));
}

// Test |BubbleFrame| when the bubble's direction is
// |BubbleArrowDirectionUp|, the alignment is |BubbleAlignmentTrailing|, and the
// language is RTL.
TEST_F(BubbleUtilTest, BubbleFrameUpTrailingRTL) {
  CGRect bubbleFrame = bubble_util::BubbleFrame(
      centerAlignedAnchorPoint_, bubbleSize_, BubbleArrowDirectionUp,
      BubbleAlignmentTrailing, containerSize_.width, true /* isRTL */);
  EXPECT_FLOAT_EQ(300.0f - TestBubbleAlignmentOffset(), bubbleFrame.origin.x);
  EXPECT_FLOAT_EQ(250.0f, bubbleFrame.origin.y);
  EXPECT_FLOAT_EQ(bubbleSize_.width, CGRectGetWidth(bubbleFrame));
  EXPECT_FLOAT_EQ(bubbleSize_.height, CGRectGetHeight(bubbleFrame));
}

// Test |BubbleFrame| when the bubble's direction is
// |BubbleArrowDirectionDown|, the alignment is |BubbleAlignmentLeading|, and
// the language is LTR.
TEST_F(BubbleUtilTest, BubbleFrameDownLeadingLTR) {
  CGRect bubbleFrame = bubble_util::BubbleFrame(
      centerAlignedAnchorPoint_, bubbleSize_, BubbleArrowDirectionDown,
      BubbleAlignmentLeading, containerSize_.width, false /* isRTL */);
  EXPECT_FLOAT_EQ(300.0f - TestBubbleAlignmentOffset(), bubbleFrame.origin.x);
  EXPECT_FLOAT_EQ(150.0f, bubbleFrame.origin.y);
  EXPECT_FLOAT_EQ(bubbleSize_.width, CGRectGetWidth(bubbleFrame));
  EXPECT_FLOAT_EQ(bubbleSize_.height, CGRectGetHeight(bubbleFrame));
}

// Test |BubbleFrame| when the bubble's direction is
// |BubbleArrowDirectionDown|, the alignment is |BubbleAlignmentLeading|, and
// the language is RTL.
TEST_F(BubbleUtilTest, BubbleFrameDownLeadingRTL) {
  CGRect bubbleFrame = bubble_util::BubbleFrame(
      centerAlignedAnchorPoint_, bubbleSize_, BubbleArrowDirectionDown,
      BubbleAlignmentLeading, containerSize_.width, true /* isRTL */);
  EXPECT_FLOAT_EQ(TestBubbleAlignmentOffset(), bubbleFrame.origin.x);
  EXPECT_FLOAT_EQ(150.0f, bubbleFrame.origin.y);
  EXPECT_FLOAT_EQ(bubbleSize_.width, CGRectGetWidth(bubbleFrame));
  EXPECT_FLOAT_EQ(bubbleSize_.height, CGRectGetHeight(bubbleFrame));
}

// Test |BubbleFrame| when the bubble's direction is
// |BubbleArrowDirectionDown|, the alignment is |BubbleAlignmentCenter|, and the
// language is LTR.
TEST_F(BubbleUtilTest, BubbleFrameDownCenteredLTR) {
  CGRect bubbleFrame = bubble_util::BubbleFrame(
      centerAlignedAnchorPoint_, bubbleSize_, BubbleArrowDirectionDown,
      BubbleAlignmentCenter, containerSize_.width, false /* isRTL */);
  EXPECT_FLOAT_EQ(150.0f, bubbleFrame.origin.x);
  EXPECT_FLOAT_EQ(150.0f, bubbleFrame.origin.y);
  EXPECT_FLOAT_EQ(bubbleSize_.width, CGRectGetWidth(bubbleFrame));
  EXPECT_FLOAT_EQ(bubbleSize_.height, CGRectGetHeight(bubbleFrame));
}

// Test |BubbleFrame| when the bubble's direction is
// |BubbleArrowDirectionDown|, the alignment is |BubbleAlignmentCenter|, and the
// language is RTL.
TEST_F(BubbleUtilTest, BubbleFrameDownCenteredRTL) {
  CGRect bubbleFrame = bubble_util::BubbleFrame(
      centerAlignedAnchorPoint_, bubbleSize_, BubbleArrowDirectionDown,
      BubbleAlignmentCenter, containerSize_.width, true /* isRTL */);
  EXPECT_FLOAT_EQ(150.0f, bubbleFrame.origin.x);
  EXPECT_FLOAT_EQ(150.0f, bubbleFrame.origin.y);
  EXPECT_FLOAT_EQ(bubbleSize_.width, CGRectGetWidth(bubbleFrame));
  EXPECT_FLOAT_EQ(bubbleSize_.height, CGRectGetHeight(bubbleFrame));
}

// Test |BubbleFrame| when the bubble's direction is
// |BubbleArrowDirectionDown|, the alignment is |BubbleAlignmentTrailing|, and
// the language is LTR.
TEST_F(BubbleUtilTest, BubbleFrameDownTrailingLTR) {
  CGRect bubbleFrame = bubble_util::BubbleFrame(
      centerAlignedAnchorPoint_, bubbleSize_, BubbleArrowDirectionDown,
      BubbleAlignmentTrailing, containerSize_.width, false /* isRTL */);
  EXPECT_FLOAT_EQ(TestBubbleAlignmentOffset(), bubbleFrame.origin.x);
  EXPECT_FLOAT_EQ(150.0f, bubbleFrame.origin.y);
  EXPECT_FLOAT_EQ(bubbleSize_.width, CGRectGetWidth(bubbleFrame));
  EXPECT_FLOAT_EQ(bubbleSize_.height, CGRectGetHeight(bubbleFrame));
}

// Test |BubbleFrame| when the bubble's direction is
// |BubbleArrowDirectionDown|, the alignment is |BubbleAlignmentTrailing|, and
// the language is RTL.
TEST_F(BubbleUtilTest, BubbleFrameDownTrailingRTL) {
  CGRect bubbleFrame = bubble_util::BubbleFrame(
      centerAlignedAnchorPoint_, bubbleSize_, BubbleArrowDirectionDown,
      BubbleAlignmentTrailing, containerSize_.width, true /* isRTL */);
  EXPECT_FLOAT_EQ(300.0f - TestBubbleAlignmentOffset(), bubbleFrame.origin.x);
  EXPECT_FLOAT_EQ(150.0f, bubbleFrame.origin.y);
  EXPECT_FLOAT_EQ(bubbleSize_.width, CGRectGetWidth(bubbleFrame));
  EXPECT_FLOAT_EQ(bubbleSize_.height, CGRectGetHeight(bubbleFrame));
}
