// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bubble/bubble_util.h"

#include "base/i18n/rtl.h"
#include "base/logging.h"
#import "ios/chrome/browser/ui/util/rtl_geometry.h"
#include "ios/chrome/browser/ui/util/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Calculate the distance from the bubble's leading edge to the leading edge of
// its bounding coordinate system. In LTR contexts, the returned float is the
// x-coordinate of the bubble's origin. This calculation is based on
// |anchorPoint|, which is the point of the target UI element the bubble is
// anchored at, and the bubble's alignment, direction, and size. The returned
// float is in the same coordinate system as |anchorPoint|, which should be the
// coordinate system in which the bubble is drawn.
CGFloat LeadingDistance(CGPoint anchorPoint,
                        BubbleAlignment alignment,
                        CGFloat bubbleWidth,
                        CGFloat boundingWidth,
                        bool isRTL) {
  // Find |leadingOffset|, the distance from the bubble's leading edge to the
  // anchor point. This depends on alignment and bubble width.
  CGFloat leadingOffset;
  switch (alignment) {
    case BubbleAlignmentLeading:
      leadingOffset = bubble_util::BubbleAlignmentOffset();
      break;
    case BubbleAlignmentCenter:
      leadingOffset = bubbleWidth / 2.0f;
      break;
    case BubbleAlignmentTrailing:
      leadingOffset = bubbleWidth - bubble_util::BubbleAlignmentOffset();
      break;
    default:
      NOTREACHED() << "Invalid bubble alignment " << alignment;
      break;
  }
  if (isRTL) {
    return boundingWidth - (anchorPoint.x + leadingOffset);
  } else {
    return anchorPoint.x - leadingOffset;
  }
}

// Calculate the y-coordinate of the bubble's origin based on |anchorPoint|, the
// point of the UI element the bubble is anchored at, and the bubble's arrow
// direction and size. The returned float is in the same coordinate system as
// |anchorPoint|, which should be the coordinate system in which the bubble is
// drawn.
CGFloat OriginY(CGPoint anchorPoint,
                BubbleArrowDirection arrowDirection,
                CGFloat bubbleHeight) {
  CGFloat originY;
  if (arrowDirection == BubbleArrowDirectionUp) {
    originY = anchorPoint.y;
  } else {
    DCHECK_EQ(arrowDirection, BubbleArrowDirectionDown);
    originY = anchorPoint.y - bubbleHeight;
  }
  return originY;
}

// Calculate the maximum width of the bubble such that it stays within its
// bounding coordinate space. |anchorPointX| is the x-coordinate of the point on
// the target UI element the bubble is anchored at. It is in the coordinate
// system in which the bubble is drawn. |alignment| is the bubble's alignment,
// |boundingWidth| is the width of the coordinate space in which the bubble is
// drawn, and |isRTL| is true if the language is RTL and |false| otherwise.
CGFloat BubbleMaxWidth(CGFloat anchorPointX,
                       BubbleAlignment alignment,
                       CGFloat boundingWidth,
                       bool isRTL) {
  CGFloat maxWidth;
  switch (alignment) {
    case BubbleAlignmentLeading:
      if (isRTL) {
        // The bubble is aligned right, and can use space to the left of the
        // anchor point and within |BubbleAlignmentOffset()| from the right.
        maxWidth = anchorPointX + bubble_util::BubbleAlignmentOffset();
      } else {
        // The bubble is aligned left, and can use space to the right of the
        // anchor point and within |BubbleAlignmentOffset()| from the left.
        maxWidth =
            boundingWidth - anchorPointX + bubble_util::BubbleAlignmentOffset();
      }
      break;
    case BubbleAlignmentCenter:
      // The width of half the bubble cannot exceed the distance from the anchor
      // point to the closest edge of the superview.
      maxWidth = MIN(anchorPointX, boundingWidth - anchorPointX) * 2.0f;
      break;
    case BubbleAlignmentTrailing:
      if (isRTL) {
        // The bubble is aligned left, and can use space to the right of the
        // anchor point and within |BubbleAlignmentOffset()| from the left.
        maxWidth =
            boundingWidth - anchorPointX + bubble_util::BubbleAlignmentOffset();
      } else {
        // The bubble is aligned right, and can use space to the left of the
        // anchor point and within |BubbleAlignmentOffset()| from the right.
        maxWidth = anchorPointX + bubble_util::BubbleAlignmentOffset();
      }
      break;
    default:
      NOTREACHED() << "Invalid bubble alignment " << alignment;
      break;
  }
  return maxWidth;
}

// Calculate the maximum height of the bubble such that it stays within its
// bounding coordinate space. |anchorPointY| is the y-coordinate of the point on
// the target UI element the bubble is anchored at. It is in the coordinate
// system in which the bubble is drawn. |direction| is the direction the arrow
// is pointing. |boundingHeight| is the height of the coordinate space in which
// the bubble is drawn.
CGFloat BubbleMaxHeight(CGFloat anchorPointY,
                        BubbleArrowDirection direction,
                        CGFloat boundingHeight) {
  CGFloat maxHeight;
  switch (direction) {
    case BubbleArrowDirectionUp:
      maxHeight = boundingHeight - anchorPointY;
      break;
    case BubbleArrowDirectionDown:
      maxHeight = anchorPointY;
      break;
    default:
      NOTREACHED() << "Invalid bubble direction " << direction;
      break;
  }
  return maxHeight;
}

}  // namespace

namespace bubble_util {

CGFloat BubbleAlignmentOffset() {
  // This is used to replace a constant that would change based on the flag.
  return 29;
}

CGPoint AnchorPoint(CGRect targetFrame, BubbleArrowDirection arrowDirection) {
  CGPoint anchorPoint;
  anchorPoint.x = CGRectGetMidX(targetFrame);
  if (arrowDirection == BubbleArrowDirectionUp) {
    anchorPoint.y = CGRectGetMaxY(targetFrame);
    return anchorPoint;
  }
  DCHECK_EQ(arrowDirection, BubbleArrowDirectionDown);
  anchorPoint.y = CGRectGetMinY(targetFrame);
  return anchorPoint;
}

// Calculate the maximum size of the bubble such that it stays within its
// superview's bounding coordinate space and does not overlap the other side of
// the anchor point. |anchorPoint| is the point on the target UI element the
// bubble is anchored at in the bubble's superview's coordinate system.
// |direction| is the bubble's direction. |alignment| is the bubble's alignment.
// |boundingSize| is the size of the superview. |isRTL| is |true| if the
// coordinates are in right-to-left language coordinates and |false| otherwise.
// This method is unit tested so it cannot be in the above anonymous namespace.
CGSize BubbleMaxSize(CGPoint anchorPoint,
                     BubbleArrowDirection direction,
                     BubbleAlignment alignment,
                     CGSize boundingSize,
                     bool isRTL) {
  CGFloat maxWidth =
      BubbleMaxWidth(anchorPoint.x, alignment, boundingSize.width, isRTL);
  CGFloat maxHeight =
      BubbleMaxHeight(anchorPoint.y, direction, boundingSize.height);
  return CGSizeMake(maxWidth, maxHeight);
}

CGSize BubbleMaxSize(CGPoint anchorPoint,
                     BubbleArrowDirection direction,
                     BubbleAlignment alignment,
                     CGSize boundingSize) {
  bool isRTL = base::i18n::IsRTL();
  return BubbleMaxSize(anchorPoint, direction, alignment, boundingSize, isRTL);
}

// Calculate the bubble's frame. |anchorPoint| is the point on the UI element
// the bubble is pointing to. |size| is the size of the bubble. |direction| is
// the direction the bubble's arrow is pointing. |alignment| is the alignment of
// the anchor (either leading, centered, or trailing). |boundingWidth| is the
// width of the bubble's superview. |isRTL| is |true| if the coordinates are in
// right-to-left language coordinates and |false| otherwise. This method is unit
// tested so it cannot be in the above anonymous namespace.
CGRect BubbleFrame(CGPoint anchorPoint,
                   CGSize size,
                   BubbleArrowDirection direction,
                   BubbleAlignment alignment,
                   CGFloat boundingWidth,
                   bool isRTL) {
  CGFloat leading =
      LeadingDistance(anchorPoint, alignment, size.width, boundingWidth, isRTL);
  CGFloat originY = OriginY(anchorPoint, direction, size.height);
  // Use a |LayoutRect| to ensure that the bubble is mirrored in RTL contexts.
  base::i18n::TextDirection textDirection =
      isRTL ? base::i18n::RIGHT_TO_LEFT : base::i18n::LEFT_TO_RIGHT;
  CGRect bubbleFrame = LayoutRectGetRectUsingDirection(
      LayoutRectMake(leading, boundingWidth, originY, size.width, size.height),
      textDirection);
  return bubbleFrame;
}

CGRect BubbleFrame(CGPoint anchorPoint,
                   CGSize size,
                   BubbleArrowDirection direction,
                   BubbleAlignment alignment,
                   CGFloat boundingWidth) {
  bool isRTL = base::i18n::IsRTL();
  return BubbleFrame(anchorPoint, size, direction, alignment, boundingWidth,
                     isRTL);
}

}  // namespace bubble_util
