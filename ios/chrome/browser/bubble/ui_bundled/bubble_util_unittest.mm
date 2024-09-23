// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bubble/ui_bundled/bubble_util.h"

#import <CoreGraphics/CoreGraphics.h>

#import "ios/chrome/browser/bubble/ui_bundled/bubble_constants.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace bubble_util {
CGSize BubbleMaxSize(CGPoint anchor_point,
                     CGFloat alignment_offset,
                     BubbleArrowDirection direction,
                     BubbleAlignment alignment,
                     CGSize bounding_size,
                     bool is_rtl);

CGRect BubbleFrame(CGPoint anchor_point,
                   CGFloat alignment_offset,
                   CGSize size,
                   BubbleArrowDirection direction,
                   BubbleAlignment alignment,
                   CGFloat bounding_width,
                   bool is_rtl);

CGFloat FloatingArrowAlignmentOffset(CGFloat bounding_width,
                                     CGPoint anchor_point,
                                     BubbleAlignment alignment);

}  // namespace bubble_util

class BubbleUtilTest : public PlatformTest {
 public:
  BubbleUtilTest()
      : left_aligned_anchor_point_({70.0f, 250.0f}),
        center_aligned_anchor_point_({250.0f, 250.0f}),
        right_aligned_anchor_point_({320.0f, 250.0f}),
        bubble_size_({250.0f, 100.0f}),
        container_size_({400.0f, 600.0f}),
        bubble_alignment_offset_(29.0f) {}

 protected:
  // Anchor point on the left side of the container.
  const CGPoint left_aligned_anchor_point_;
  // Anchor point on the center of the container.
  const CGPoint center_aligned_anchor_point_;
  // Anchor point on the right side of the container.
  const CGPoint right_aligned_anchor_point_;
  // Size of the bubble.
  const CGSize bubble_size_;
  // Bounding size of the bubble's coordinate system.
  const CGSize container_size_;
  // Distance from the anchor point to the `BubbleAlignment` edge of the
  // bubble's frame.
  const CGFloat bubble_alignment_offset_;
};

// Test the `AnchorPoint` method when the arrow is pointing upwards, meaning the
// bubble is below the UI element.
TEST_F(BubbleUtilTest, AnchorPointUp) {
  CGPoint anchor_point = bubble_util::AnchorPoint(
      {{250.0f, 200.0f}, {100.0f, 100.0f}}, BubbleArrowDirectionUp);
  EXPECT_TRUE(CGPointEqualToPoint({300.0f, 300.0f}, anchor_point));
}

// Test the `AnchorPoint` method when the arrow is pointing downwards, meaning
// the bubble is above the UI element.
TEST_F(BubbleUtilTest, AnchorPointDown) {
  CGPoint anchor_point = bubble_util::AnchorPoint(
      {{250.0f, 200.0f}, {100.0f, 100.0f}}, BubbleArrowDirectionDown);
  EXPECT_TRUE(CGPointEqualToPoint({300.0f, 200.0f}, anchor_point));
}

// Test the `BubbleMaxSize` method when the bubble is leading aligned, the
// target is on the left side of the container, the bubble is pointing up, and
// the language is LTR.
TEST_F(BubbleUtilTest, BubbleMaxSizeLeadingWithTargetOnLeftUp) {
  CGSize left_aligned_size = bubble_util::BubbleMaxSize(
      left_aligned_anchor_point_, bubble_alignment_offset_,
      BubbleArrowDirectionUp, BubbleAlignmentTopOrLeading, container_size_,
      false /* isRTL */);

  EXPECT_FLOAT_EQ(330.0f + bubble_alignment_offset_, left_aligned_size.width);
  EXPECT_FLOAT_EQ(350.0f, left_aligned_size.height);
}

// Test the `BubbleMaxSize` method when the bubble is leading aligned, the
// target is on the center of the container, the bubble is pointing down, and
// the language is LTR.
TEST_F(BubbleUtilTest, BubbleMaxSizeLeadingWithTargetOnCenterDown) {
  CGSize center_aligned_size = bubble_util::BubbleMaxSize(
      center_aligned_anchor_point_, bubble_alignment_offset_,
      BubbleArrowDirectionDown, BubbleAlignmentTopOrLeading, container_size_,
      false /* isRTL */);

  EXPECT_FLOAT_EQ(150.0f + bubble_alignment_offset_, center_aligned_size.width);
  EXPECT_FLOAT_EQ(250.0f, center_aligned_size.height);
}

// Test the `BubbleMaxSize` method when the bubble is leading aligned, the
// target is on the right side of the container, the bubble is pointing up, and
// the language is LTR.
TEST_F(BubbleUtilTest, BubbleMaxSizeLeadingWithTargetOnRightUp) {
  CGSize right_aligned_size = bubble_util::BubbleMaxSize(
      right_aligned_anchor_point_, bubble_alignment_offset_,
      BubbleArrowDirectionUp, BubbleAlignmentTopOrLeading, container_size_,
      false /* isRTL */);

  EXPECT_FLOAT_EQ(80.0f + bubble_alignment_offset_, right_aligned_size.width);
  EXPECT_FLOAT_EQ(350.0f, right_aligned_size.height);
}

// Test the `BubbleMaxSize` method when the bubble is center aligned, the target
// is on the left side of the container, the bubble is pointing down, and the
// language is LTR.
TEST_F(BubbleUtilTest, BubbleMaxSizeCenterWithTargetOnLeftDown) {
  CGSize leftAlignedSize = bubble_util::BubbleMaxSize(
      left_aligned_anchor_point_, bubble_alignment_offset_,
      BubbleArrowDirectionDown, BubbleAlignmentCenter, container_size_,
      false /* isRTL */);

  EXPECT_FLOAT_EQ(140.0f, leftAlignedSize.width);
  EXPECT_FLOAT_EQ(250.0f, leftAlignedSize.height);
}

// Test the `BubbleMaxSize` method when the bubble is center aligned, the target
// is on the center of the container, the bubble is pointing up, and the
// language is LTR.
TEST_F(BubbleUtilTest, BubbleMaxSizeCenterWithTargetOnCenterUp) {
  CGSize centerAlignedSize = bubble_util::BubbleMaxSize(
      center_aligned_anchor_point_, bubble_alignment_offset_,
      BubbleArrowDirectionUp, BubbleAlignmentCenter, container_size_,
      false /* isRTL */);

  EXPECT_FLOAT_EQ(300.0f, centerAlignedSize.width);
  EXPECT_FLOAT_EQ(350.0f, centerAlignedSize.height);
}

// Test the `BubbleMaxSize` method when the bubble is center aligned, the target
// is on the right side of the container, the bubble is pointing down, and the
// language is LTR.
TEST_F(BubbleUtilTest, BubbleMaxSizeCenterWithTargetOnRightDown) {
  CGSize right_aligned_size = bubble_util::BubbleMaxSize(
      right_aligned_anchor_point_, bubble_alignment_offset_,
      BubbleArrowDirectionDown, BubbleAlignmentCenter, container_size_,
      false /* isRTL */);

  EXPECT_FLOAT_EQ(160.0f, right_aligned_size.width);
  EXPECT_FLOAT_EQ(250.0f, right_aligned_size.height);
}

// Test the `BubbleMaxSize` method when the bubble is trailing aligned, the
// target is on the left side of the container, the bubble is pointing up, and
// the language is LTR.
TEST_F(BubbleUtilTest, BubbleMaxSizeTrailingWithTargetOnLeftUp) {
  CGSize leftAlignedSize = bubble_util::BubbleMaxSize(
      left_aligned_anchor_point_, bubble_alignment_offset_,
      BubbleArrowDirectionUp, BubbleAlignmentBottomOrTrailing, container_size_,
      false /* isRTL */);

  EXPECT_FLOAT_EQ(70.0f + bubble_alignment_offset_, leftAlignedSize.width);
  EXPECT_FLOAT_EQ(350.0f, leftAlignedSize.height);
}

// Test the `BubbleMaxSize` method when the bubble is trailing aligned, the
// target is on the center of the container, the bubble is pointing down, and
// the language is LTR.
TEST_F(BubbleUtilTest, BubbleMaxSizeTrailingWithTargetOnCenterDown) {
  CGSize centerAlignedSize = bubble_util::BubbleMaxSize(
      center_aligned_anchor_point_, bubble_alignment_offset_,
      BubbleArrowDirectionDown, BubbleAlignmentBottomOrTrailing,
      container_size_, false /* isRTL */);

  EXPECT_FLOAT_EQ(250.0f + bubble_alignment_offset_, centerAlignedSize.width);
  EXPECT_FLOAT_EQ(250.0f, centerAlignedSize.height);
}

// Test the `BubbleMaxSize` method when the bubble is trailing aligned, the
// target is on the right side of the container, the bubble is pointing up, and
// the language is LTR.
TEST_F(BubbleUtilTest, BubbleMaxSizeTrailingWithTargetOnRightUp) {
  CGSize right_aligned_size = bubble_util::BubbleMaxSize(
      right_aligned_anchor_point_, bubble_alignment_offset_,
      BubbleArrowDirectionUp, BubbleAlignmentBottomOrTrailing, container_size_,
      false /* isRTL */);

  EXPECT_FLOAT_EQ(320.0f + bubble_alignment_offset_, right_aligned_size.width);
  EXPECT_FLOAT_EQ(350.0f, right_aligned_size.height);
}

// Test the `BubbleMaxSize` method when the bubble is leading aligned, the
// target is on the left side of the container, the bubble is pointing down, and
// the language is RTL.
TEST_F(BubbleUtilTest, BubbleMaxSizeLeadingWithTargetOnLeftDownRTL) {
  CGSize left_aligned_size_rtl = bubble_util::BubbleMaxSize(
      left_aligned_anchor_point_, bubble_alignment_offset_,
      BubbleArrowDirectionDown, BubbleAlignmentTopOrLeading, container_size_,
      true /* isRTL */);

  EXPECT_FLOAT_EQ(70.0f + bubble_alignment_offset_,
                  left_aligned_size_rtl.width);
  EXPECT_FLOAT_EQ(250.0f, left_aligned_size_rtl.height);
}

// Test the `BubbleMaxSize` method when the bubble is leading aligned, the
// target is on the center of the container, the bubble is pointing up, and the
// language is RTL.
TEST_F(BubbleUtilTest, BubbleMaxSizeLeadingWithTargetOnCenterUpRTL) {
  CGSize center_aligned_size_rtl = bubble_util::BubbleMaxSize(
      center_aligned_anchor_point_, bubble_alignment_offset_,
      BubbleArrowDirectionUp, BubbleAlignmentTopOrLeading, container_size_,
      true /* isRTL */);

  EXPECT_FLOAT_EQ(250.0f + bubble_alignment_offset_,
                  center_aligned_size_rtl.width);
  EXPECT_FLOAT_EQ(350.0f, center_aligned_size_rtl.height);
}

// Test the `BubbleMaxSize` method when the bubble is leading aligned, the
// target is on the right side of the container, the bubble is pointing down,
// and the language is RTL.
TEST_F(BubbleUtilTest, BubbleMaxSizeLeadingWithTargetOnRightDownRTL) {
  CGSize right_aligned_size_rtl = bubble_util::BubbleMaxSize(
      right_aligned_anchor_point_, bubble_alignment_offset_,
      BubbleArrowDirectionDown, BubbleAlignmentTopOrLeading, container_size_,
      true /* isRTL */);

  EXPECT_FLOAT_EQ(320.0f + bubble_alignment_offset_,
                  right_aligned_size_rtl.width);
  EXPECT_FLOAT_EQ(250.0f, right_aligned_size_rtl.height);
}

// Test the `BubbleMaxSize` method when the bubble is center aligned, the target
// is on the left side of the container, the bubble is pointing up, and the
// language is RTL.
TEST_F(BubbleUtilTest, BubbleMaxSizeCenterWithTargetOnLeftUpRTL) {
  CGSize left_aligned_size_rtl = bubble_util::BubbleMaxSize(
      left_aligned_anchor_point_, bubble_alignment_offset_,
      BubbleArrowDirectionUp, BubbleAlignmentCenter, container_size_,
      true /* isRTL */);

  EXPECT_FLOAT_EQ(140.0f, left_aligned_size_rtl.width);
  EXPECT_FLOAT_EQ(350.0f, left_aligned_size_rtl.height);
}

// Test the `BubbleMaxSize` method when the bubble is center aligned, the target
// is on the center of the container, the bubble is pointing down, and the
// language is RTL.
TEST_F(BubbleUtilTest, BubbleMaxSizeCenterWithTargetOnCenterDownRTL) {
  CGSize center_aligned_size_rtl = bubble_util::BubbleMaxSize(
      center_aligned_anchor_point_, bubble_alignment_offset_,
      BubbleArrowDirectionDown, BubbleAlignmentCenter, container_size_,
      true /* isRTL */);

  EXPECT_FLOAT_EQ(300.0f, center_aligned_size_rtl.width);
  EXPECT_FLOAT_EQ(250.0f, center_aligned_size_rtl.height);
}

// Test the `BubbleMaxSize` method when the bubble is center aligned, the target
// is on the right side of the container, the bubble is pointing up, and the
// language is RTL.
TEST_F(BubbleUtilTest, BubbleMaxSizeCenterWithTargetOnRightUpRTL) {
  CGSize right_aligned_size_rtl = bubble_util::BubbleMaxSize(
      right_aligned_anchor_point_, bubble_alignment_offset_,
      BubbleArrowDirectionUp, BubbleAlignmentCenter, container_size_,
      true /* isRTL */);

  EXPECT_FLOAT_EQ(160.0f, right_aligned_size_rtl.width);
  EXPECT_FLOAT_EQ(350.0f, right_aligned_size_rtl.height);
}

// Test the `BubbleMaxSize` method when the bubble is trailing aligned, the
// target is on the left side of the container, the bubble is pointing down, and
// the language is RTL.
TEST_F(BubbleUtilTest, BubbleMaxSizeTrailingWithTargetOnLeftDownRTL) {
  CGSize left_aligned_size_rtl = bubble_util::BubbleMaxSize(
      left_aligned_anchor_point_, bubble_alignment_offset_,
      BubbleArrowDirectionDown, BubbleAlignmentBottomOrTrailing,
      container_size_, true /* isRTL */);

  EXPECT_FLOAT_EQ(330.0f + bubble_alignment_offset_,
                  left_aligned_size_rtl.width);
  EXPECT_FLOAT_EQ(250.0f, left_aligned_size_rtl.height);
}

// Test the `BubbleMaxSize` method when the bubble is trailing aligned, the
// target is on the center of the container, the bubble is pointing up, and the
// language is RTL.
TEST_F(BubbleUtilTest, BubbleMaxSizeTrailingWithTargetOnCenterUpRTL) {
  CGSize center_aligned_size_rtl = bubble_util::BubbleMaxSize(
      center_aligned_anchor_point_, bubble_alignment_offset_,
      BubbleArrowDirectionUp, BubbleAlignmentBottomOrTrailing, container_size_,
      true /* isRTL */);

  EXPECT_FLOAT_EQ(150.0f + bubble_alignment_offset_,
                  center_aligned_size_rtl.width);
  EXPECT_FLOAT_EQ(350.0f, center_aligned_size_rtl.height);
}

// Test the `BubbleMaxSize` method when the bubble is trailing aligned, the
// target is on the right side of the container, the bubble is pointing down,
// and the language is RTL.
TEST_F(BubbleUtilTest, BubbleMaxSizeTrailingWithTargetOnRightDownRTL) {
  CGSize right_aligned_size_rtl = bubble_util::BubbleMaxSize(
      right_aligned_anchor_point_, bubble_alignment_offset_,
      BubbleArrowDirectionDown, BubbleAlignmentBottomOrTrailing,
      container_size_, true /* isRTL */);

  EXPECT_FLOAT_EQ(80.0f + bubble_alignment_offset_,
                  right_aligned_size_rtl.width);
  EXPECT_FLOAT_EQ(250.0f, right_aligned_size_rtl.height);
}

// Test `BubbleFrame` when the bubble's direction is
// `BubbleArrowDirectionUp`, the alignment is `BubbleAlignmentTopOrLeading`, and
// the language is LTR.
TEST_F(BubbleUtilTest, BubbleFrameUpLeadingLTR) {
  CGRect bubble_frame = bubble_util::BubbleFrame(
      center_aligned_anchor_point_, bubble_alignment_offset_, bubble_size_,
      BubbleArrowDirectionUp, BubbleAlignmentTopOrLeading,
      container_size_.width, false /* isRTL */);
  EXPECT_FLOAT_EQ(250.0f - bubble_alignment_offset_, bubble_frame.origin.x);
  EXPECT_FLOAT_EQ(250.0f, bubble_frame.origin.y);
  EXPECT_FLOAT_EQ(bubble_size_.width, CGRectGetWidth(bubble_frame));
  EXPECT_FLOAT_EQ(bubble_size_.height, CGRectGetHeight(bubble_frame));
}

// Test `BubbleFrame` when the bubble's direction is
// `BubbleArrowDirectionUp`, the alignment is `BubbleAlignmentTopOrLeading`, and
// the language is RTL.
TEST_F(BubbleUtilTest, BubbleFrameUpLeadingRTL) {
  CGRect bubble_frame = bubble_util::BubbleFrame(
      center_aligned_anchor_point_, bubble_alignment_offset_, bubble_size_,
      BubbleArrowDirectionUp, BubbleAlignmentTopOrLeading,
      container_size_.width, true /* isRTL */);
  EXPECT_FLOAT_EQ(bubble_alignment_offset_, bubble_frame.origin.x);
  EXPECT_FLOAT_EQ(250.0f, bubble_frame.origin.y);
  EXPECT_FLOAT_EQ(bubble_size_.width, CGRectGetWidth(bubble_frame));
  EXPECT_FLOAT_EQ(bubble_size_.height, CGRectGetHeight(bubble_frame));
}

// Test `BubbleFrame` when the bubble's direction is
// `BubbleArrowDirectionUp`, the alignment is `BubbleAlignmentCenter`, and the
// language is LTR.
TEST_F(BubbleUtilTest, BubbleFrameUpCenteredLTR) {
  CGRect bubble_frame = bubble_util::BubbleFrame(
      center_aligned_anchor_point_, bubble_alignment_offset_, bubble_size_,
      BubbleArrowDirectionUp, BubbleAlignmentCenter, container_size_.width,
      false /* isRTL */);
  EXPECT_FLOAT_EQ(125.0f, bubble_frame.origin.x);
  EXPECT_FLOAT_EQ(250.0f, bubble_frame.origin.y);
  EXPECT_FLOAT_EQ(bubble_size_.width, CGRectGetWidth(bubble_frame));
  EXPECT_FLOAT_EQ(bubble_size_.height, CGRectGetHeight(bubble_frame));
}

// Test `BubbleFrame` when the bubble's direction is
// `BubbleArrowDirectionUp`, the alignment is `BubbleAlignmentCenter`, and the
// language is RTL.
TEST_F(BubbleUtilTest, BubbleFrameUpCenteredRTL) {
  CGRect bubble_frame = bubble_util::BubbleFrame(
      center_aligned_anchor_point_, bubble_alignment_offset_, bubble_size_,
      BubbleArrowDirectionUp, BubbleAlignmentCenter, container_size_.width,
      true /* isRTL */);
  EXPECT_FLOAT_EQ(125.0f, bubble_frame.origin.x);
  EXPECT_FLOAT_EQ(250.0f, bubble_frame.origin.y);
  EXPECT_FLOAT_EQ(bubble_size_.width, CGRectGetWidth(bubble_frame));
  EXPECT_FLOAT_EQ(bubble_size_.height, CGRectGetHeight(bubble_frame));
}

// Test `BubbleFrame` when the bubble's direction is
// `BubbleArrowDirectionUp`, the alignment is `BubbleAlignmentBottomOrTrailing`,
// and the language is LTR.
TEST_F(BubbleUtilTest, BubbleFrameUpTrailingLTR) {
  CGRect bubble_frame = bubble_util::BubbleFrame(
      center_aligned_anchor_point_, bubble_alignment_offset_, bubble_size_,
      BubbleArrowDirectionUp, BubbleAlignmentBottomOrTrailing,
      container_size_.width, false /* isRTL */);
  EXPECT_FLOAT_EQ(bubble_alignment_offset_, bubble_frame.origin.x);
  EXPECT_FLOAT_EQ(250.0f, bubble_frame.origin.y);
  EXPECT_FLOAT_EQ(bubble_size_.width, CGRectGetWidth(bubble_frame));
  EXPECT_FLOAT_EQ(bubble_size_.height, CGRectGetHeight(bubble_frame));
}

// Test `BubbleFrame` when the bubble's direction is
// `BubbleArrowDirectionUp`, the alignment is `BubbleAlignmentBottomOrTrailing`,
// and the language is RTL.
TEST_F(BubbleUtilTest, BubbleFrameUpTrailingRTL) {
  CGRect bubble_frame = bubble_util::BubbleFrame(
      center_aligned_anchor_point_, bubble_alignment_offset_, bubble_size_,
      BubbleArrowDirectionUp, BubbleAlignmentBottomOrTrailing,
      container_size_.width, true /* isRTL */);
  EXPECT_FLOAT_EQ(250.0f - bubble_alignment_offset_, bubble_frame.origin.x);
  EXPECT_FLOAT_EQ(250.0f, bubble_frame.origin.y);
  EXPECT_FLOAT_EQ(bubble_size_.width, CGRectGetWidth(bubble_frame));
  EXPECT_FLOAT_EQ(bubble_size_.height, CGRectGetHeight(bubble_frame));
}

// Test `BubbleFrame` when the bubble's direction is
// `BubbleArrowDirectionDown`, the alignment is `BubbleAlignmentTopOrLeading`,
// and the language is LTR.
TEST_F(BubbleUtilTest, BubbleFrameDownLeadingLTR) {
  CGRect bubble_frame = bubble_util::BubbleFrame(
      center_aligned_anchor_point_, bubble_alignment_offset_, bubble_size_,
      BubbleArrowDirectionDown, BubbleAlignmentTopOrLeading,
      container_size_.width, false /* isRTL */);
  EXPECT_FLOAT_EQ(250.0f - bubble_alignment_offset_, bubble_frame.origin.x);
  EXPECT_FLOAT_EQ(150.0f, bubble_frame.origin.y);
  EXPECT_FLOAT_EQ(bubble_size_.width, CGRectGetWidth(bubble_frame));
  EXPECT_FLOAT_EQ(bubble_size_.height, CGRectGetHeight(bubble_frame));
}

// Test `BubbleFrame` when the bubble's direction is
// `BubbleArrowDirectionDown`, the alignment is `BubbleAlignmentTopOrLeading`,
// and the language is RTL.
TEST_F(BubbleUtilTest, BubbleFrameDownLeadingRTL) {
  CGRect bubble_frame = bubble_util::BubbleFrame(
      center_aligned_anchor_point_, bubble_alignment_offset_, bubble_size_,
      BubbleArrowDirectionDown, BubbleAlignmentTopOrLeading,
      container_size_.width, true /* isRTL */);
  EXPECT_FLOAT_EQ(bubble_alignment_offset_, bubble_frame.origin.x);
  EXPECT_FLOAT_EQ(150.0f, bubble_frame.origin.y);
  EXPECT_FLOAT_EQ(bubble_size_.width, CGRectGetWidth(bubble_frame));
  EXPECT_FLOAT_EQ(bubble_size_.height, CGRectGetHeight(bubble_frame));
}

// Test `BubbleFrame` when the bubble's direction is
// `BubbleArrowDirectionDown`, the alignment is `BubbleAlignmentCenter`, and the
// language is LTR.
TEST_F(BubbleUtilTest, BubbleFrameDownCenteredLTR) {
  CGRect bubble_frame = bubble_util::BubbleFrame(
      center_aligned_anchor_point_, bubble_alignment_offset_, bubble_size_,
      BubbleArrowDirectionDown, BubbleAlignmentCenter, container_size_.width,
      false /* isRTL */);
  EXPECT_FLOAT_EQ(125.0f, bubble_frame.origin.x);
  EXPECT_FLOAT_EQ(150.0f, bubble_frame.origin.y);
  EXPECT_FLOAT_EQ(bubble_size_.width, CGRectGetWidth(bubble_frame));
  EXPECT_FLOAT_EQ(bubble_size_.height, CGRectGetHeight(bubble_frame));
}

// Test `BubbleFrame` when the bubble's direction is
// `BubbleArrowDirectionDown`, the alignment is `BubbleAlignmentCenter`, and the
// language is RTL.
TEST_F(BubbleUtilTest, BubbleFrameDownCenteredRTL) {
  CGRect bubble_frame = bubble_util::BubbleFrame(
      center_aligned_anchor_point_, bubble_alignment_offset_, bubble_size_,
      BubbleArrowDirectionDown, BubbleAlignmentCenter, container_size_.width,
      true /* isRTL */);
  EXPECT_FLOAT_EQ(125.0f, bubble_frame.origin.x);
  EXPECT_FLOAT_EQ(150.0f, bubble_frame.origin.y);
  EXPECT_FLOAT_EQ(bubble_size_.width, CGRectGetWidth(bubble_frame));
  EXPECT_FLOAT_EQ(bubble_size_.height, CGRectGetHeight(bubble_frame));
}

// Test `BubbleFrame` when the bubble's direction is
// `BubbleArrowDirectionDown`, the alignment is
// `BubbleAlignmentBottomOrTrailing`, and the language is LTR.
TEST_F(BubbleUtilTest, BubbleFrameDownTrailingLTR) {
  CGRect bubble_frame = bubble_util::BubbleFrame(
      center_aligned_anchor_point_, bubble_alignment_offset_, bubble_size_,
      BubbleArrowDirectionDown, BubbleAlignmentBottomOrTrailing,
      container_size_.width, false /* isRTL */);
  EXPECT_FLOAT_EQ(bubble_alignment_offset_, bubble_frame.origin.x);
  EXPECT_FLOAT_EQ(150.0f, bubble_frame.origin.y);
  EXPECT_FLOAT_EQ(bubble_size_.width, CGRectGetWidth(bubble_frame));
  EXPECT_FLOAT_EQ(bubble_size_.height, CGRectGetHeight(bubble_frame));
}

// Test `BubbleFrame` when the bubble's direction is
// `BubbleArrowDirectionDown`, the alignment is
// `BubbleAlignmentBottomOrTrailing`, and the language is RTL.
TEST_F(BubbleUtilTest, BubbleFrameDownTrailingRTL) {
  CGRect bubble_frame = bubble_util::BubbleFrame(
      center_aligned_anchor_point_, bubble_alignment_offset_, bubble_size_,
      BubbleArrowDirectionDown, BubbleAlignmentBottomOrTrailing,
      container_size_.width, true /* isRTL */);
  EXPECT_FLOAT_EQ(250.0f - bubble_alignment_offset_, bubble_frame.origin.x);
  EXPECT_FLOAT_EQ(150.0f, bubble_frame.origin.y);
  EXPECT_FLOAT_EQ(bubble_size_.width, CGRectGetWidth(bubble_frame));
  EXPECT_FLOAT_EQ(bubble_size_.height, CGRectGetHeight(bubble_frame));
}

// Tests FloatingArrowAlignmentOffset with a left anchor point and a leading
// alignment.
TEST_F(BubbleUtilTest, FloatingArrowAlignmentOffsetLeadingLeft) {
  CGFloat alignment_offset = bubble_util::FloatingArrowAlignmentOffset(
      container_size_.width, left_aligned_anchor_point_,
      BubbleAlignmentTopOrLeading);
  EXPECT_FLOAT_EQ(left_aligned_anchor_point_.x, alignment_offset);
}

// Tests FloatingArrowAlignmentOffset with a center anchor point and a center
// alignment.
TEST_F(BubbleUtilTest, FloatingArrowAlignmentOffsetCenter) {
  CGFloat alignment_offset = bubble_util::FloatingArrowAlignmentOffset(
      container_size_.width, center_aligned_anchor_point_,
      BubbleAlignmentCenter);
  // Bubble is center aligned, the `alignment_offset` is ignored, it's set to
  // the minimum of `BubbleDefaultalignment_offset`.
  EXPECT_FLOAT_EQ(29.0f, alignment_offset);
}

// Tests FloatingArrowAlignmentOffset with a right anchor point and a trailing
// alignment.
TEST_F(BubbleUtilTest, FloatingArrowAlignmentOffsetTrailing) {
  CGFloat alignment_offset = bubble_util::FloatingArrowAlignmentOffset(
      container_size_.width, right_aligned_anchor_point_,
      BubbleAlignmentBottomOrTrailing);
  EXPECT_FLOAT_EQ(container_size_.width - right_aligned_anchor_point_.x,
                  alignment_offset);
}
