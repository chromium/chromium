// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bubble/bubble_util.h"

#import <CoreGraphics/CoreGraphics.h>

#import "ios/chrome/browser/ui/bubble/bubble_constants.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace bubble_util {
CGSize BubbleMaxSize(CGPoint anchorPoint,
                     CGFloat alignmentOffset,
                     BubbleArrowDirection direction,
                     BubbleAlignment alignment,
                     CGSize boundingSize,
                     bool isRTL);

CGRect BubbleFrame(CGPoint anchorPoint,
                   CGFloat alignmentOffset,
                   CGSize size,
                   BubbleArrowDirection direction,
                   BubbleAlignment alignment,
                   CGFloat boundingWidth,
                   bool isRTL);

CGFloat FloatingArrowAlignmentOffset(CGFloat boundingWidth,
                                     CGPoint anchorPoint,
                                     BubbleAlignment alignment);

}  // namespace bubble_util

class BubbleUtilTest : public PlatformTest {
 public:
  BubbleUtilTest()
      : leftAlignedAnchorPoint_({70.0f, 250.0f}),
        centerAlignedAnchorPoint_({250.0f, 250.0f}),
        rightAlignedAnchorPoint_({320.0f, 250.0f}),
        bubbleSize_({250.0f, 100.0f}),
        containerSize_({400.0f, 600.0f}),
        bubbleAlignmentOffset_(29.0f) {}

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
  // Distance from the anchor point to the `BubbleAlignment` edge of the
  // bubble's frame.
  const CGFloat bubbleAlignmentOffset_;
};

// Test the `AnchorPoint` method when the arrow is pointing upwards, meaning the
// bubble is below the UI element.
TEST_F(BubbleUtilTest, AnchorPointUp) {
  CGPoint anchorPoint = bubble_util::AnchorPoint(
      {{250.0f, 200.0f}, {100.0f, 100.0f}}, BubbleArrowDirectionUp);
  EXPECT_TRUE(CGPointEqualToPoint({300.0f, 300.0f}, anchorPoint));
}

// Test the `AnchorPoint` method when the arrow is pointing downwards, meaning
// the bubble is above the UI element.
TEST_F(BubbleUtilTest, AnchorPointDown) {
  CGPoint anchorPoint = bubble_util::AnchorPoint(
      {{250.0f, 200.0f}, {100.0f, 100.0f}}, BubbleArrowDirectionDown);
  EXPECT_TRUE(CGPointEqualToPoint({300.0f, 200.0f}, anchorPoint));
}

// Test the `BubbleMaxSize` method when the bubble is leading aligned, the
// target is on the left side of the container, the bubble is pointing up, and
// the language is LTR.
TEST_F(BubbleUtilTest, BubbleMaxSizeLeadingWithTargetOnLeftUp) {
  CGSize leftAlignedSize = bubble_util::BubbleMaxSize(
      leftAlignedAnchorPoint_, bubbleAlignmentOffset_, BubbleArrowDirectionUp,
      BubbleAlignmentTopOrLeading, containerSize_, false /* isRTL */);

  EXPECT_FLOAT_EQ(330.0f + bubbleAlignmentOffset_, leftAlignedSize.width);
  EXPECT_FLOAT_EQ(350.0f, leftAlignedSize.height);
}

// Test the `BubbleMaxSize` method when the bubble is leading aligned, the
// target is on the center of the container, the bubble is pointing down, and
// the language is LTR.
TEST_F(BubbleUtilTest, BubbleMaxSizeLeadingWithTargetOnCenterDown) {
  CGSize centerAlignedSize = bubble_util::BubbleMaxSize(
      centerAlignedAnchorPoint_, bubbleAlignmentOffset_,
      BubbleArrowDirectionDown, BubbleAlignmentTopOrLeading, containerSize_,
      false /* isRTL */);

  EXPECT_FLOAT_EQ(150.0f + bubbleAlignmentOffset_, centerAlignedSize.width);
  EXPECT_FLOAT_EQ(250.0f, centerAlignedSize.height);
}

// Test the `BubbleMaxSize` method when the bubble is leading aligned, the
// target is on the right side of the container, the bubble is pointing up, and
// the language is LTR.
TEST_F(BubbleUtilTest, BubbleMaxSizeLeadingWithTargetOnRightUp) {
  CGSize rightAlignedSize = bubble_util::BubbleMaxSize(
      rightAlignedAnchorPoint_, bubbleAlignmentOffset_, BubbleArrowDirectionUp,
      BubbleAlignmentTopOrLeading, containerSize_, false /* isRTL */);

  EXPECT_FLOAT_EQ(80.0f + bubbleAlignmentOffset_, rightAlignedSize.width);
  EXPECT_FLOAT_EQ(350.0f, rightAlignedSize.height);
}

// Test the `BubbleMaxSize` method when the bubble is center aligned, the target
// is on the left side of the container, the bubble is pointing down, and the
// language is LTR.
TEST_F(BubbleUtilTest, BubbleMaxSizeCenterWithTargetOnLeftDown) {
  CGSize leftAlignedSize = bubble_util::BubbleMaxSize(
      leftAlignedAnchorPoint_, bubbleAlignmentOffset_, BubbleArrowDirectionDown,
      BubbleAlignmentCenter, containerSize_, false /* isRTL */);

  EXPECT_FLOAT_EQ(140.0f, leftAlignedSize.width);
  EXPECT_FLOAT_EQ(250.0f, leftAlignedSize.height);
}

// Test the `BubbleMaxSize` method when the bubble is center aligned, the target
// is on the center of the container, the bubble is pointing up, and the
// language is LTR.
TEST_F(BubbleUtilTest, BubbleMaxSizeCenterWithTargetOnCenterUp) {
  CGSize centerAlignedSize = bubble_util::BubbleMaxSize(
      centerAlignedAnchorPoint_, bubbleAlignmentOffset_, BubbleArrowDirectionUp,
      BubbleAlignmentCenter, containerSize_, false /* isRTL */);

  EXPECT_FLOAT_EQ(300.0f, centerAlignedSize.width);
  EXPECT_FLOAT_EQ(350.0f, centerAlignedSize.height);
}

// Test the `BubbleMaxSize` method when the bubble is center aligned, the target
// is on the right side of the container, the bubble is pointing down, and the
// language is LTR.
TEST_F(BubbleUtilTest, BubbleMaxSizeCenterWithTargetOnRightDown) {
  CGSize rightAlignedSize = bubble_util::BubbleMaxSize(
      rightAlignedAnchorPoint_, bubbleAlignmentOffset_,
      BubbleArrowDirectionDown, BubbleAlignmentCenter, containerSize_,
      false /* isRTL */);

  EXPECT_FLOAT_EQ(160.0f, rightAlignedSize.width);
  EXPECT_FLOAT_EQ(250.0f, rightAlignedSize.height);
}

// Test the `BubbleMaxSize` method when the bubble is trailing aligned, the
// target is on the left side of the container, the bubble is pointing up, and
// the language is LTR.
TEST_F(BubbleUtilTest, BubbleMaxSizeTrailingWithTargetOnLeftUp) {
  CGSize leftAlignedSize = bubble_util::BubbleMaxSize(
      leftAlignedAnchorPoint_, bubbleAlignmentOffset_, BubbleArrowDirectionUp,
      BubbleAlignmentBottomOrTrailing, containerSize_, false /* isRTL */);

  EXPECT_FLOAT_EQ(70.0f + bubbleAlignmentOffset_, leftAlignedSize.width);
  EXPECT_FLOAT_EQ(350.0f, leftAlignedSize.height);
}

// Test the `BubbleMaxSize` method when the bubble is trailing aligned, the
// target is on the center of the container, the bubble is pointing down, and
// the language is LTR.
TEST_F(BubbleUtilTest, BubbleMaxSizeTrailingWithTargetOnCenterDown) {
  CGSize centerAlignedSize = bubble_util::BubbleMaxSize(
      centerAlignedAnchorPoint_, bubbleAlignmentOffset_,
      BubbleArrowDirectionDown, BubbleAlignmentBottomOrTrailing, containerSize_,
      false /* isRTL */);

  EXPECT_FLOAT_EQ(250.0f + bubbleAlignmentOffset_, centerAlignedSize.width);
  EXPECT_FLOAT_EQ(250.0f, centerAlignedSize.height);
}

// Test the `BubbleMaxSize` method when the bubble is trailing aligned, the
// target is on the right side of the container, the bubble is pointing up, and
// the language is LTR.
TEST_F(BubbleUtilTest, BubbleMaxSizeTrailingWithTargetOnRightUp) {
  CGSize rightAlignedSize = bubble_util::BubbleMaxSize(
      rightAlignedAnchorPoint_, bubbleAlignmentOffset_, BubbleArrowDirectionUp,
      BubbleAlignmentBottomOrTrailing, containerSize_, false /* isRTL */);

  EXPECT_FLOAT_EQ(320.0f + bubbleAlignmentOffset_, rightAlignedSize.width);
  EXPECT_FLOAT_EQ(350.0f, rightAlignedSize.height);
}

// Test the `BubbleMaxSize` method when the bubble is leading aligned, the
// target is on the left side of the container, the bubble is pointing down, and
// the language is RTL.
TEST_F(BubbleUtilTest, BubbleMaxSizeLeadingWithTargetOnLeftDownRTL) {
  CGSize leftAlignedSizeRTL = bubble_util::BubbleMaxSize(
      leftAlignedAnchorPoint_, bubbleAlignmentOffset_, BubbleArrowDirectionDown,
      BubbleAlignmentTopOrLeading, containerSize_, true /* isRTL */);

  EXPECT_FLOAT_EQ(70.0f + bubbleAlignmentOffset_, leftAlignedSizeRTL.width);
  EXPECT_FLOAT_EQ(250.0f, leftAlignedSizeRTL.height);
}

// Test the `BubbleMaxSize` method when the bubble is leading aligned, the
// target is on the center of the container, the bubble is pointing up, and the
// language is RTL.
TEST_F(BubbleUtilTest, BubbleMaxSizeLeadingWithTargetOnCenterUpRTL) {
  CGSize centerAlignedSizeRTL = bubble_util::BubbleMaxSize(
      centerAlignedAnchorPoint_, bubbleAlignmentOffset_, BubbleArrowDirectionUp,
      BubbleAlignmentTopOrLeading, containerSize_, true /* isRTL */);

  EXPECT_FLOAT_EQ(250.0f + bubbleAlignmentOffset_, centerAlignedSizeRTL.width);
  EXPECT_FLOAT_EQ(350.0f, centerAlignedSizeRTL.height);
}

// Test the `BubbleMaxSize` method when the bubble is leading aligned, the
// target is on the right side of the container, the bubble is pointing down,
// and the language is RTL.
TEST_F(BubbleUtilTest, BubbleMaxSizeLeadingWithTargetOnRightDownRTL) {
  CGSize rightAlignedSizeRTL = bubble_util::BubbleMaxSize(
      rightAlignedAnchorPoint_, bubbleAlignmentOffset_,
      BubbleArrowDirectionDown, BubbleAlignmentTopOrLeading, containerSize_,
      true /* isRTL */);

  EXPECT_FLOAT_EQ(320.0f + bubbleAlignmentOffset_, rightAlignedSizeRTL.width);
  EXPECT_FLOAT_EQ(250.0f, rightAlignedSizeRTL.height);
}

// Test the `BubbleMaxSize` method when the bubble is center aligned, the target
// is on the left side of the container, the bubble is pointing up, and the
// language is RTL.
TEST_F(BubbleUtilTest, BubbleMaxSizeCenterWithTargetOnLeftUpRTL) {
  CGSize leftAlignedSizeRTL = bubble_util::BubbleMaxSize(
      leftAlignedAnchorPoint_, bubbleAlignmentOffset_, BubbleArrowDirectionUp,
      BubbleAlignmentCenter, containerSize_, true /* isRTL */);

  EXPECT_FLOAT_EQ(140.0f, leftAlignedSizeRTL.width);
  EXPECT_FLOAT_EQ(350.0f, leftAlignedSizeRTL.height);
}

// Test the `BubbleMaxSize` method when the bubble is center aligned, the target
// is on the center of the container, the bubble is pointing down, and the
// language is RTL.
TEST_F(BubbleUtilTest, BubbleMaxSizeCenterWithTargetOnCenterDownRTL) {
  CGSize centerAlignedSizeRTL = bubble_util::BubbleMaxSize(
      centerAlignedAnchorPoint_, bubbleAlignmentOffset_,
      BubbleArrowDirectionDown, BubbleAlignmentCenter, containerSize_,
      true /* isRTL */);

  EXPECT_FLOAT_EQ(300.0f, centerAlignedSizeRTL.width);
  EXPECT_FLOAT_EQ(250.0f, centerAlignedSizeRTL.height);
}

// Test the `BubbleMaxSize` method when the bubble is center aligned, the target
// is on the right side of the container, the bubble is pointing up, and the
// language is RTL.
TEST_F(BubbleUtilTest, BubbleMaxSizeCenterWithTargetOnRightUpRTL) {
  CGSize rightAlignedSizeRTL = bubble_util::BubbleMaxSize(
      rightAlignedAnchorPoint_, bubbleAlignmentOffset_, BubbleArrowDirectionUp,
      BubbleAlignmentCenter, containerSize_, true /* isRTL */);

  EXPECT_FLOAT_EQ(160.0f, rightAlignedSizeRTL.width);
  EXPECT_FLOAT_EQ(350.0f, rightAlignedSizeRTL.height);
}

// Test the `BubbleMaxSize` method when the bubble is trailing aligned, the
// target is on the left side of the container, the bubble is pointing down, and
// the language is RTL.
TEST_F(BubbleUtilTest, BubbleMaxSizeTrailingWithTargetOnLeftDownRTL) {
  CGSize leftAlignedSizeRTL = bubble_util::BubbleMaxSize(
      leftAlignedAnchorPoint_, bubbleAlignmentOffset_, BubbleArrowDirectionDown,
      BubbleAlignmentBottomOrTrailing, containerSize_, true /* isRTL */);

  EXPECT_FLOAT_EQ(330.0f + bubbleAlignmentOffset_, leftAlignedSizeRTL.width);
  EXPECT_FLOAT_EQ(250.0f, leftAlignedSizeRTL.height);
}

// Test the `BubbleMaxSize` method when the bubble is trailing aligned, the
// target is on the center of the container, the bubble is pointing up, and the
// language is RTL.
TEST_F(BubbleUtilTest, BubbleMaxSizeTrailingWithTargetOnCenterUpRTL) {
  CGSize centerAlignedSizeRTL = bubble_util::BubbleMaxSize(
      centerAlignedAnchorPoint_, bubbleAlignmentOffset_, BubbleArrowDirectionUp,
      BubbleAlignmentBottomOrTrailing, containerSize_, true /* isRTL */);

  EXPECT_FLOAT_EQ(150.0f + bubbleAlignmentOffset_, centerAlignedSizeRTL.width);
  EXPECT_FLOAT_EQ(350.0f, centerAlignedSizeRTL.height);
}

// Test the `BubbleMaxSize` method when the bubble is trailing aligned, the
// target is on the right side of the container, the bubble is pointing down,
// and the language is RTL.
TEST_F(BubbleUtilTest, BubbleMaxSizeTrailingWithTargetOnRightDownRTL) {
  CGSize rightAlignedSizeRTL = bubble_util::BubbleMaxSize(
      rightAlignedAnchorPoint_, bubbleAlignmentOffset_,
      BubbleArrowDirectionDown, BubbleAlignmentBottomOrTrailing, containerSize_,
      true /* isRTL */);

  EXPECT_FLOAT_EQ(80.0f + bubbleAlignmentOffset_, rightAlignedSizeRTL.width);
  EXPECT_FLOAT_EQ(250.0f, rightAlignedSizeRTL.height);
}

// Test `BubbleFrame` when the bubble's direction is
// `BubbleArrowDirectionUp`, the alignment is `BubbleAlignmentTopOrLeading`, and
// the language is LTR.
TEST_F(BubbleUtilTest, BubbleFrameUpLeadingLTR) {
  CGRect bubbleFrame = bubble_util::BubbleFrame(
      centerAlignedAnchorPoint_, bubbleAlignmentOffset_, bubbleSize_,
      BubbleArrowDirectionUp, BubbleAlignmentTopOrLeading, containerSize_.width,
      false /* isRTL */);
  EXPECT_FLOAT_EQ(250.0f - bubbleAlignmentOffset_, bubbleFrame.origin.x);
  EXPECT_FLOAT_EQ(250.0f, bubbleFrame.origin.y);
  EXPECT_FLOAT_EQ(bubbleSize_.width, CGRectGetWidth(bubbleFrame));
  EXPECT_FLOAT_EQ(bubbleSize_.height, CGRectGetHeight(bubbleFrame));
}

// Test `BubbleFrame` when the bubble's direction is
// `BubbleArrowDirectionUp`, the alignment is `BubbleAlignmentTopOrLeading`, and
// the language is RTL.
TEST_F(BubbleUtilTest, BubbleFrameUpLeadingRTL) {
  CGRect bubbleFrame = bubble_util::BubbleFrame(
      centerAlignedAnchorPoint_, bubbleAlignmentOffset_, bubbleSize_,
      BubbleArrowDirectionUp, BubbleAlignmentTopOrLeading, containerSize_.width,
      true /* isRTL */);
  EXPECT_FLOAT_EQ(bubbleAlignmentOffset_, bubbleFrame.origin.x);
  EXPECT_FLOAT_EQ(250.0f, bubbleFrame.origin.y);
  EXPECT_FLOAT_EQ(bubbleSize_.width, CGRectGetWidth(bubbleFrame));
  EXPECT_FLOAT_EQ(bubbleSize_.height, CGRectGetHeight(bubbleFrame));
}

// Test `BubbleFrame` when the bubble's direction is
// `BubbleArrowDirectionUp`, the alignment is `BubbleAlignmentCenter`, and the
// language is LTR.
TEST_F(BubbleUtilTest, BubbleFrameUpCenteredLTR) {
  CGRect bubbleFrame = bubble_util::BubbleFrame(
      centerAlignedAnchorPoint_, bubbleAlignmentOffset_, bubbleSize_,
      BubbleArrowDirectionUp, BubbleAlignmentCenter, containerSize_.width,
      false /* isRTL */);
  EXPECT_FLOAT_EQ(125.0f, bubbleFrame.origin.x);
  EXPECT_FLOAT_EQ(250.0f, bubbleFrame.origin.y);
  EXPECT_FLOAT_EQ(bubbleSize_.width, CGRectGetWidth(bubbleFrame));
  EXPECT_FLOAT_EQ(bubbleSize_.height, CGRectGetHeight(bubbleFrame));
}

// Test `BubbleFrame` when the bubble's direction is
// `BubbleArrowDirectionUp`, the alignment is `BubbleAlignmentCenter`, and the
// language is RTL.
TEST_F(BubbleUtilTest, BubbleFrameUpCenteredRTL) {
  CGRect bubbleFrame = bubble_util::BubbleFrame(
      centerAlignedAnchorPoint_, bubbleAlignmentOffset_, bubbleSize_,
      BubbleArrowDirectionUp, BubbleAlignmentCenter, containerSize_.width,
      true /* isRTL */);
  EXPECT_FLOAT_EQ(125.0f, bubbleFrame.origin.x);
  EXPECT_FLOAT_EQ(250.0f, bubbleFrame.origin.y);
  EXPECT_FLOAT_EQ(bubbleSize_.width, CGRectGetWidth(bubbleFrame));
  EXPECT_FLOAT_EQ(bubbleSize_.height, CGRectGetHeight(bubbleFrame));
}

// Test `BubbleFrame` when the bubble's direction is
// `BubbleArrowDirectionUp`, the alignment is `BubbleAlignmentBottomOrTrailing`,
// and the language is LTR.
TEST_F(BubbleUtilTest, BubbleFrameUpTrailingLTR) {
  CGRect bubbleFrame = bubble_util::BubbleFrame(
      centerAlignedAnchorPoint_, bubbleAlignmentOffset_, bubbleSize_,
      BubbleArrowDirectionUp, BubbleAlignmentBottomOrTrailing,
      containerSize_.width, false /* isRTL */);
  EXPECT_FLOAT_EQ(bubbleAlignmentOffset_, bubbleFrame.origin.x);
  EXPECT_FLOAT_EQ(250.0f, bubbleFrame.origin.y);
  EXPECT_FLOAT_EQ(bubbleSize_.width, CGRectGetWidth(bubbleFrame));
  EXPECT_FLOAT_EQ(bubbleSize_.height, CGRectGetHeight(bubbleFrame));
}

// Test `BubbleFrame` when the bubble's direction is
// `BubbleArrowDirectionUp`, the alignment is `BubbleAlignmentBottomOrTrailing`,
// and the language is RTL.
TEST_F(BubbleUtilTest, BubbleFrameUpTrailingRTL) {
  CGRect bubbleFrame = bubble_util::BubbleFrame(
      centerAlignedAnchorPoint_, bubbleAlignmentOffset_, bubbleSize_,
      BubbleArrowDirectionUp, BubbleAlignmentBottomOrTrailing,
      containerSize_.width, true /* isRTL */);
  EXPECT_FLOAT_EQ(250.0f - bubbleAlignmentOffset_, bubbleFrame.origin.x);
  EXPECT_FLOAT_EQ(250.0f, bubbleFrame.origin.y);
  EXPECT_FLOAT_EQ(bubbleSize_.width, CGRectGetWidth(bubbleFrame));
  EXPECT_FLOAT_EQ(bubbleSize_.height, CGRectGetHeight(bubbleFrame));
}

// Test `BubbleFrame` when the bubble's direction is
// `BubbleArrowDirectionDown`, the alignment is `BubbleAlignmentTopOrLeading`,
// and the language is LTR.
TEST_F(BubbleUtilTest, BubbleFrameDownLeadingLTR) {
  CGRect bubbleFrame = bubble_util::BubbleFrame(
      centerAlignedAnchorPoint_, bubbleAlignmentOffset_, bubbleSize_,
      BubbleArrowDirectionDown, BubbleAlignmentTopOrLeading,
      containerSize_.width, false /* isRTL */);
  EXPECT_FLOAT_EQ(250.0f - bubbleAlignmentOffset_, bubbleFrame.origin.x);
  EXPECT_FLOAT_EQ(150.0f, bubbleFrame.origin.y);
  EXPECT_FLOAT_EQ(bubbleSize_.width, CGRectGetWidth(bubbleFrame));
  EXPECT_FLOAT_EQ(bubbleSize_.height, CGRectGetHeight(bubbleFrame));
}

// Test `BubbleFrame` when the bubble's direction is
// `BubbleArrowDirectionDown`, the alignment is `BubbleAlignmentTopOrLeading`,
// and the language is RTL.
TEST_F(BubbleUtilTest, BubbleFrameDownLeadingRTL) {
  CGRect bubbleFrame = bubble_util::BubbleFrame(
      centerAlignedAnchorPoint_, bubbleAlignmentOffset_, bubbleSize_,
      BubbleArrowDirectionDown, BubbleAlignmentTopOrLeading,
      containerSize_.width, true /* isRTL */);
  EXPECT_FLOAT_EQ(bubbleAlignmentOffset_, bubbleFrame.origin.x);
  EXPECT_FLOAT_EQ(150.0f, bubbleFrame.origin.y);
  EXPECT_FLOAT_EQ(bubbleSize_.width, CGRectGetWidth(bubbleFrame));
  EXPECT_FLOAT_EQ(bubbleSize_.height, CGRectGetHeight(bubbleFrame));
}

// Test `BubbleFrame` when the bubble's direction is
// `BubbleArrowDirectionDown`, the alignment is `BubbleAlignmentCenter`, and the
// language is LTR.
TEST_F(BubbleUtilTest, BubbleFrameDownCenteredLTR) {
  CGRect bubbleFrame = bubble_util::BubbleFrame(
      centerAlignedAnchorPoint_, bubbleAlignmentOffset_, bubbleSize_,
      BubbleArrowDirectionDown, BubbleAlignmentCenter, containerSize_.width,
      false /* isRTL */);
  EXPECT_FLOAT_EQ(125.0f, bubbleFrame.origin.x);
  EXPECT_FLOAT_EQ(150.0f, bubbleFrame.origin.y);
  EXPECT_FLOAT_EQ(bubbleSize_.width, CGRectGetWidth(bubbleFrame));
  EXPECT_FLOAT_EQ(bubbleSize_.height, CGRectGetHeight(bubbleFrame));
}

// Test `BubbleFrame` when the bubble's direction is
// `BubbleArrowDirectionDown`, the alignment is `BubbleAlignmentCenter`, and the
// language is RTL.
TEST_F(BubbleUtilTest, BubbleFrameDownCenteredRTL) {
  CGRect bubbleFrame = bubble_util::BubbleFrame(
      centerAlignedAnchorPoint_, bubbleAlignmentOffset_, bubbleSize_,
      BubbleArrowDirectionDown, BubbleAlignmentCenter, containerSize_.width,
      true /* isRTL */);
  EXPECT_FLOAT_EQ(125.0f, bubbleFrame.origin.x);
  EXPECT_FLOAT_EQ(150.0f, bubbleFrame.origin.y);
  EXPECT_FLOAT_EQ(bubbleSize_.width, CGRectGetWidth(bubbleFrame));
  EXPECT_FLOAT_EQ(bubbleSize_.height, CGRectGetHeight(bubbleFrame));
}

// Test `BubbleFrame` when the bubble's direction is
// `BubbleArrowDirectionDown`, the alignment is
// `BubbleAlignmentBottomOrTrailing`, and the language is LTR.
TEST_F(BubbleUtilTest, BubbleFrameDownTrailingLTR) {
  CGRect bubbleFrame = bubble_util::BubbleFrame(
      centerAlignedAnchorPoint_, bubbleAlignmentOffset_, bubbleSize_,
      BubbleArrowDirectionDown, BubbleAlignmentBottomOrTrailing,
      containerSize_.width, false /* isRTL */);
  EXPECT_FLOAT_EQ(bubbleAlignmentOffset_, bubbleFrame.origin.x);
  EXPECT_FLOAT_EQ(150.0f, bubbleFrame.origin.y);
  EXPECT_FLOAT_EQ(bubbleSize_.width, CGRectGetWidth(bubbleFrame));
  EXPECT_FLOAT_EQ(bubbleSize_.height, CGRectGetHeight(bubbleFrame));
}

// Test `BubbleFrame` when the bubble's direction is
// `BubbleArrowDirectionDown`, the alignment is
// `BubbleAlignmentBottomOrTrailing`, and the language is RTL.
TEST_F(BubbleUtilTest, BubbleFrameDownTrailingRTL) {
  CGRect bubbleFrame = bubble_util::BubbleFrame(
      centerAlignedAnchorPoint_, bubbleAlignmentOffset_, bubbleSize_,
      BubbleArrowDirectionDown, BubbleAlignmentBottomOrTrailing,
      containerSize_.width, true /* isRTL */);
  EXPECT_FLOAT_EQ(250.0f - bubbleAlignmentOffset_, bubbleFrame.origin.x);
  EXPECT_FLOAT_EQ(150.0f, bubbleFrame.origin.y);
  EXPECT_FLOAT_EQ(bubbleSize_.width, CGRectGetWidth(bubbleFrame));
  EXPECT_FLOAT_EQ(bubbleSize_.height, CGRectGetHeight(bubbleFrame));
}

// Tests FloatingArrowAlignmentOffset with a left anchor point and a leading
// alignment.
TEST_F(BubbleUtilTest, FloatingArrowAlignmentOffsetLeadingLeft) {
  CGFloat alignmentOffset = bubble_util::FloatingArrowAlignmentOffset(
      containerSize_.width, leftAlignedAnchorPoint_,
      BubbleAlignmentTopOrLeading);
  EXPECT_FLOAT_EQ(leftAlignedAnchorPoint_.x, alignmentOffset);
}

// Tests FloatingArrowAlignmentOffset with a center anchor point and a center
// alignment.
TEST_F(BubbleUtilTest, FloatingArrowAlignmentOffsetCenter) {
  CGFloat alignmentOffset = bubble_util::FloatingArrowAlignmentOffset(
      containerSize_.width, centerAlignedAnchorPoint_, BubbleAlignmentCenter);
  // Bubble is center aligned, the `alignmentOffset` is ignored, it's set to the
  // minimum of `BubbleDefaultAlignmentOffset`.
  EXPECT_FLOAT_EQ(29.0f, alignmentOffset);
}

// Tests FloatingArrowAlignmentOffset with a right anchor point and a trailing
// alignment.
TEST_F(BubbleUtilTest, FloatingArrowAlignmentOffsetTrailing) {
  CGFloat alignmentOffset = bubble_util::FloatingArrowAlignmentOffset(
      containerSize_.width, rightAlignedAnchorPoint_,
      BubbleAlignmentBottomOrTrailing);
  EXPECT_FLOAT_EQ(containerSize_.width - rightAlignedAnchorPoint_.x,
                  alignmentOffset);
}
