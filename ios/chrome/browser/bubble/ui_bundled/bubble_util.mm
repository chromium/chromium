// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bubble/ui_bundled/bubble_util.h"

#import <ostream>

#import "base/check_op.h"
#import "base/i18n/rtl.h"
#import "base/notreached.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_constants.h"

namespace {

// Bubble's maximum width, preserves readability, ensuring that the bubble does
// not span across wide screens.
const CGFloat kBubbleMaxWidth = 375.0f;

// Whether bubble with arrow direction `direction` is pointing left.
BOOL IsArrowPointingLeft(BubbleArrowDirection direction, bool is_rtl) {
  return direction ==
         (is_rtl ? BubbleArrowDirectionTrailing : BubbleArrowDirectionLeading);
}

// Calculate the distance from the bubble's leading edge to the leading edge of
// its bounding coordinate system. In LTR contexts, the returned float is the
// x-coordinate of the bubble's origin. This calculation is based on
// `anchor_point`, which is the point of the target UI element the bubble is
// anchored at, and the bubble's alignment offset, alignment, direction, and
// size. The returned float is in the same coordinate system as `anchor_point`,
// which should be the coordinate system in which the bubble is drawn.
CGFloat LeadingDistance(CGPoint anchor_point,
                        BubbleArrowDirection arrow_direction,
                        CGFloat bubble_alignment_offset,
                        BubbleAlignment alignment,
                        CGFloat bubble_width,
                        CGFloat bounding_width,
                        bool is_rtl) {
  // Find `leading_offset`, the distance from the bubble's leading edge to the
  // anchor point. This depends on alignment and bubble width.
  CGFloat leading_offset;
  switch (arrow_direction) {
    case BubbleArrowDirectionUp:
    case BubbleArrowDirectionDown:
      switch (alignment) {
        case BubbleAlignmentTopOrLeading:
          leading_offset = bubble_alignment_offset;
          break;
        case BubbleAlignmentCenter:
          leading_offset = bubble_width / 2.0f;
          break;
        case BubbleAlignmentBottomOrTrailing:
          leading_offset = bubble_width - bubble_alignment_offset;
          break;
        default:
          NOTREACHED_IN_MIGRATION() << "Invalid bubble alignment " << alignment;
          break;
      }
      break;
    case BubbleArrowDirectionLeading:
      leading_offset = 0;
      break;
    case BubbleArrowDirectionTrailing:
      leading_offset = bubble_width;
      break;
  }
  CGFloat leading_distance;
  if (is_rtl) {
    leading_distance = bounding_width - (anchor_point.x + leading_offset);
  } else {
    leading_distance = anchor_point.x - leading_offset;
  }
  // Round the leading distance.
  return round(leading_distance);
}

// Calculate the y-coordinate of the bubble's origin based on `anchor_point`,
// the point of the UI element the bubble is anchored at, and the bubble's
// alignment offset, alignment, direction and size. The returned float is in the
// same coordinate system as `anchor_point`, which should be the coordinate
// system in which the bubble is drawn.
CGFloat OriginY(CGPoint anchor_point,
                BubbleArrowDirection arrow_direction,
                CGFloat bubble_alignment_offset,
                BubbleAlignment alignment,
                CGFloat bubble_height) {
  CGFloat origin_y;
  switch (arrow_direction) {
    case BubbleArrowDirectionUp:
      origin_y = anchor_point.y;
      break;
    case BubbleArrowDirectionDown:
      origin_y = anchor_point.y - bubble_height;
      break;
    case BubbleArrowDirectionLeading:
    case BubbleArrowDirectionTrailing:
      switch (alignment) {
        case BubbleAlignmentTopOrLeading:
          origin_y = anchor_point.y - bubble_alignment_offset;
          break;
        case BubbleAlignmentCenter:
          origin_y = anchor_point.y - bubble_height / 2;
          break;
        case BubbleAlignmentBottomOrTrailing:
          origin_y = anchor_point.y - (bubble_height - bubble_alignment_offset);
          break;
      }
      break;
  }
  // Round down the origin Y.
  return floor(origin_y);
}

// Calculate the maximum width of the bubble such that it stays within its
// bounding coordinate space. `anchor_point_x` is the x-coordinate of the point
// on the target UI element the bubble is anchored at. It is in the coordinate
// system in which the bubble is drawn. `direction` is the direction the bubble
// arrow points to. `bubble_alignment_offset` is the distance from the leading
// edge of the bubble to the anchor point if leading aligned, and from the
// trailing edge of the bubble to the anchor point if trailing aligned.
// `alignment` is the bubble's alignment, `bounding_width` is the width of the
// coordinate space in which the bubble is drawn, and `is_rtl` is true if the
// language is RTL and `false` otherwise.
CGFloat BubbleMaxWidth(CGFloat anchor_point_x,
                       BubbleArrowDirection direction,
                       CGFloat bubble_alignment_offset,
                       BubbleAlignment alignment,
                       CGFloat bounding_width,
                       bool is_rtl) {
  CGFloat max_width;
  // Space on the left of the anchor point.
  CGFloat distance_to_left_edge = anchor_point_x;
  // Space on the right of the anchor point.
  CGFloat distance_to_right_edge = bounding_width - anchor_point_x;
  switch (direction) {
    case BubbleArrowDirectionUp:
    case BubbleArrowDirectionDown:
      switch (alignment) {
        case BubbleAlignmentTopOrLeading:
          // The bubble can use the space from the anchor point to the trailing
          // edge.
          max_width =
              (is_rtl ? distance_to_left_edge : distance_to_right_edge) +
              bubble_alignment_offset;
          break;
        case BubbleAlignmentCenter:
          // The width of half the bubble cannot exceed the distance from the
          // anchor point to the closest edge of the superview.
          max_width = MIN(distance_to_left_edge, distance_to_right_edge) * 2.0f;
          break;
        case BubbleAlignmentBottomOrTrailing:
          // The bubble can use the space from the anchor point to the leading
          // edge.
          max_width =
              (is_rtl ? distance_to_right_edge : distance_to_left_edge) +
              bubble_alignment_offset;
          break;
        default:
          NOTREACHED_IN_MIGRATION() << "Invalid bubble alignment " << alignment;
          break;
      }
      break;
    case BubbleArrowDirectionLeading:
    case BubbleArrowDirectionTrailing:
      if (IsArrowPointingLeft(direction, is_rtl)) {
        max_width = distance_to_right_edge;
      } else {
        max_width = distance_to_left_edge;
      }
      break;
  }
  // Round up the width.
  return ceil(MIN(max_width, kBubbleMaxWidth));
}

// Calculate the maximum height of the bubble such that it stays within its
// bounding coordinate space. `anchor_point_y` is the y-coordinate of the point
// on the target UI element the bubble is anchored at. It is in the coordinate
// system in which the bubble is drawn. `direction` is the direction the arrow
// is pointing. `bubble_alignment_offset` is the distance from the leading or
// top edge of the bubble to the anchor point if leading aligned, and from the
// bottom or trailing edge of the bubble to the anchor point if trailing
// aligned. `alignment` is the bubble's alignment, `bounding_height` is the
// height of the coordinate space in which the bubble is drawn.
CGFloat BubbleMaxHeight(CGFloat anchor_point_y,
                        BubbleArrowDirection direction,
                        CGFloat bubble_alignment_offset,
                        BubbleAlignment alignment,
                        CGFloat bounding_height) {
  CGFloat max_height;
  // Space on the top of the anchor point.
  CGFloat distance_to_top_edge = anchor_point_y;
  // Space on the bottom of the anchor point.
  CGFloat distance_to_bottom_edge = bounding_height - anchor_point_y;
  switch (direction) {
    case BubbleArrowDirectionUp:
      max_height = distance_to_bottom_edge;
      break;
    case BubbleArrowDirectionDown:
      max_height = distance_to_top_edge;
      break;
    case BubbleArrowDirectionLeading:
    case BubbleArrowDirectionTrailing:
      switch (alignment) {
        case BubbleAlignmentTopOrLeading:
          // The bubble can use the space from the anchor point to the bottom
          // edge.
          max_height = distance_to_bottom_edge + bubble_alignment_offset;
          break;
        case BubbleAlignmentCenter:
          // The height of half the bubble cannot exceed the distance from the
          // anchor point to the closest edge of the superview.
          max_height =
              MIN(distance_to_top_edge, distance_to_bottom_edge) * 2.0f;
          break;
        case BubbleAlignmentBottomOrTrailing:
          // The bubble can use the space from the anchor point to the top
          // edge.
          max_height = distance_to_top_edge + bubble_alignment_offset;
          break;
      }
      break;
  }
  // Round up the height.
  return ceil(max_height);
}

}  // namespace

namespace bubble_util {

CGFloat BubbleDefaultAlignmentOffset() {
  // This is used to replace a constant that would change based on the flag.
  return 29;
}

CGPoint AnchorPoint(CGRect target_frame, BubbleArrowDirection arrow_direction) {
  CGPoint anchor_point;
  bool is_rtl = base::i18n::IsRTL();
  switch (arrow_direction) {
    case BubbleArrowDirectionUp:
      anchor_point.x = CGRectGetMidX(target_frame);
      anchor_point.y = CGRectGetMaxY(target_frame);
      break;
    case BubbleArrowDirectionDown:
      anchor_point.x = CGRectGetMidX(target_frame);
      anchor_point.y = CGRectGetMinY(target_frame);
      break;
    case BubbleArrowDirectionLeading:
    case BubbleArrowDirectionTrailing:
      anchor_point.x = IsArrowPointingLeft(arrow_direction, is_rtl)
                           ? CGRectGetMaxX(target_frame)
                           : CGRectGetMinX(target_frame);
      anchor_point.y = CGRectGetMidY(target_frame);
      break;
  }
  return anchor_point;
}

// Calculate the maximum size of the bubble such that it stays within its
// superview's bounding coordinate space and does not overlap the other side of
// the anchor point. `anchor_point` is the point on the target UI element the
// bubble is anchored at in the bubble's superview's coordinate system.
// `bubble_alignment_offset` is the distance from the leading edge of the bubble
// to the anchor point if leading aligned, and from the trailing edge of the
// bubble to the anchor point if trailing aligned. `direction` is the bubble's
// direction. `alignment` is the bubble's alignment. `bounding_size` is the size
// of the superview. `is_rtl` is `true` if the coordinates are in right-to-left
// language coordinates and `false` otherwise. This method is unit tested so it
// cannot be in the above anonymous namespace.
CGSize BubbleMaxSize(CGPoint anchor_point,
                     CGFloat bubble_alignment_offset,
                     BubbleArrowDirection direction,
                     BubbleAlignment alignment,
                     CGSize bounding_size,
                     bool is_rtl) {
  CGFloat max_width =
      BubbleMaxWidth(anchor_point.x, direction, bubble_alignment_offset,
                     alignment, bounding_size.width, is_rtl);
  CGFloat max_height =
      BubbleMaxHeight(anchor_point.y, direction, bubble_alignment_offset,
                      alignment, bounding_size.height);
  return CGSizeMake(max_width, max_height);
}

CGSize BubbleMaxSize(CGPoint anchor_point,
                     CGFloat bubble_alignment_offset,
                     BubbleArrowDirection direction,
                     BubbleAlignment alignment,
                     CGSize bounding_size) {
  bool is_rtl = base::i18n::IsRTL();
  return BubbleMaxSize(anchor_point, bubble_alignment_offset, direction,
                       alignment, bounding_size, is_rtl);
}

// Calculate the bubble's frame. `anchor_point` is the point on the UI element
// the bubble is pointing to. `bubble_alignment_offset` is the distance from the
// leading edge of the bubble to the anchor point if leading aligned, and from
// the trailing edge of the bubble to the anchor point if trailing aligned.
// `size` is the size of the bubble. `direction` is the direction the bubble's
// arrow is pointing. `alignment` is the alignment of the anchor (either
// leading, centered, or trailing). `bounding_width` is the width of the
// bubble's superview. `is_rtl` is `true` if the coordinates are in
// right-to-left language coordinates and `false` otherwise. This method is unit
// tested so it cannot be in the above anonymous namespace.
CGRect BubbleFrame(CGPoint anchor_point,
                   CGFloat bubble_alignment_offset,
                   CGSize size,
                   BubbleArrowDirection direction,
                   BubbleAlignment alignment,
                   CGFloat bounding_width,
                   bool is_rtl) {
  CGFloat leading =
      LeadingDistance(anchor_point, direction, bubble_alignment_offset,
                      alignment, size.width, bounding_width, is_rtl);
  CGFloat origin_y = OriginY(anchor_point, direction, bubble_alignment_offset,
                             alignment, size.height);
  // Use a `LayoutRect` to ensure that the bubble is mirrored in RTL contexts.
  base::i18n::TextDirection textDirection =
      is_rtl ? base::i18n::RIGHT_TO_LEFT : base::i18n::LEFT_TO_RIGHT;
  CGRect bubble_frame = LayoutRectGetRectUsingDirection(
      LayoutRectMake(leading, bounding_width, origin_y, size.width,
                     size.height),
      textDirection);
  return bubble_frame;
}

CGRect BubbleFrame(CGPoint anchor_point,
                   CGFloat bubble_alignment_offset,
                   CGSize size,
                   BubbleArrowDirection direction,
                   BubbleAlignment alignment,
                   CGFloat bounding_width) {
  bool is_rtl = base::i18n::IsRTL();
  return BubbleFrame(anchor_point, bubble_alignment_offset, size, direction,
                     alignment, bounding_width, is_rtl);
}

CGFloat FloatingArrowAlignmentOffset(CGFloat bounding_width,
                                     CGPoint anchor_point,
                                     BubbleAlignment alignment) {
  CGFloat alignment_offset;
  BOOL is_rtl = base::i18n::IsRTL();
  // TODO(crbug.com/40276959): Leading and trailing direction.
  switch (alignment) {
    case BubbleAlignmentTopOrLeading:
      alignment_offset =
          is_rtl ? bounding_width - anchor_point.x : anchor_point.x;
      break;
    case BubbleAlignmentCenter:
      alignment_offset = 0.0f;  // value is ignored when laying out the arrow.
      break;
    case BubbleAlignmentBottomOrTrailing:
      alignment_offset =
          is_rtl ? anchor_point.x : bounding_width - anchor_point.x;
      break;
  }
  // Alignment offset must be greater than `BubbleDefaultAlignmentOffset` to
  // make sure the arrow is in the frame of the background of the bubble. The
  // maximum is set to the middle of the bubble so the arrow stays close to the
  // leading edge when using a leading alignment.
  return MAX(MIN(kBubbleMaxWidth / 2, alignment_offset),
             BubbleDefaultAlignmentOffset());
}

}  // namespace bubble_util
