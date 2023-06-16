// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bubble/bubble_util.h"

#import <ostream>

#import "base/check_op.h"
#import "base/i18n/rtl.h"
#import "base/notreached.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Bubble's maximum width, preserves readability, ensuring that the bubble does
// not span across wide screens.
const CGFloat kBubbleMaxWidth = 375.0f;

// Calculate the distance from the bubble's leading edge to the leading edge of
// its bounding coordinate system. In LTR contexts, the returned float is the
// x-coordinate of the bubble's origin. This calculation is based on
// `anchorPoint`, which is the point of the target UI element the bubble is
// anchored at, and the bubble's alignment offset, alignment, direction, and
// size. The returned float is in the same coordinate system as `anchorPoint`,
// which should be the coordinate system in which the bubble is drawn.
CGFloat LeadingDistance(CGPoint anchorPoint,
                        CGFloat bubbleAlignmentOffset,
                        BubbleAlignment alignment,
                        CGFloat bubbleWidth,
                        CGFloat boundingWidth,
                        bool isRTL) {
  // Find `leadingOffset`, the distance from the bubble's leading edge to the
  // anchor point. This depends on alignment and bubble width.
  CGFloat leadingOffset;
  switch (alignment) {
    case BubbleAlignmentLeading:
      leadingOffset = bubbleAlignmentOffset;
      break;
    case BubbleAlignmentCenter:
      leadingOffset = bubbleWidth / 2.0f;
      break;
    case BubbleAlignmentTrailing:
      leadingOffset = bubbleWidth - bubbleAlignmentOffset;
      break;
    default:
      NOTREACHED() << "Invalid bubble alignment " << alignment;
      break;
  }
  CGFloat leadingDistance;
  if (isRTL) {
    leadingDistance = boundingWidth - (anchorPoint.x + leadingOffset);
  } else {
    leadingDistance = anchorPoint.x - leadingOffset;
  }
  // Round the leading distance.
  return round(leadingDistance);
}

// Calculate the y-coordinate of the bubble's origin based on `anchorPoint`, the
// point of the UI element the bubble is anchored at, and the bubble's arrow
// direction and size. The returned float is in the same coordinate system as
// `anchorPoint`, which should be the coordinate system in which the bubble is
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
  // Round down the origin Y.
  return floor(originY);
}

// Calculate the maximum width of the bubble such that it stays within its
// bounding coordinate space. `anchorPointX` is the x-coordinate of the point on
// the target UI element the bubble is anchored at. It is in the coordinate
// system in which the bubble is drawn. `bubbleAlignmentOffset` is the distance
// from the leading edge of the bubble to the anchor point if leading aligned,
// and from the trailing edge of the bubble to the anchor point if trailing
// aligned. `alignment` is the bubble's alignment, `boundingWidth` is the width
// of the coordinate space in which the bubble is drawn, and `isRTL` is true if
// the language is RTL and `false` otherwise.
CGFloat BubbleMaxWidth(CGFloat anchorPointX,
                       CGFloat bubbleAlignmentOffset,
                       BubbleAlignment alignment,
                       CGFloat boundingWidth,
                       bool isRTL) {
  CGFloat maxWidth;
  switch (alignment) {
    case BubbleAlignmentLeading:
      if (isRTL) {
        // The bubble is aligned right, and can use space to the left of the
        // anchor point and within `BubbleAlignmentOffset()` from the right.
        maxWidth = anchorPointX + bubbleAlignmentOffset;
      } else {
        // The bubble is aligned left, and can use space to the right of the
        // anchor point and within `BubbleAlignmentOffset()` from the left.
        maxWidth = boundingWidth - anchorPointX + bubbleAlignmentOffset;
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
        // anchor point and within `BubbleAlignmentOffset()` from the left.
        maxWidth = boundingWidth - anchorPointX + bubbleAlignmentOffset;
      } else {
        // The bubble is aligned right, and can use space to the left of the
        // anchor point and within `BubbleAlignmentOffset()` from the right.
        maxWidth = anchorPointX + bubbleAlignmentOffset;
      }
      break;
    default:
      NOTREACHED() << "Invalid bubble alignment " << alignment;
      break;
  }
  // Round up the width.
  return ceil(MIN(maxWidth, kBubbleMaxWidth));
}

// Calculate the maximum height of the bubble such that it stays within its
// bounding coordinate space. `anchorPointY` is the y-coordinate of the point on
// the target UI element the bubble is anchored at. It is in the coordinate
// system in which the bubble is drawn. `direction` is the direction the arrow
// is pointing. `boundingHeight` is the height of the coordinate space in which
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
  // Round up the height.
  return ceil(maxHeight);
}

}  // namespace

namespace bubble_util {

CGFloat BubbleDefaultAlignmentOffset() {
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
// the anchor point. `anchorPoint` is the point on the target UI element the
// bubble is anchored at in the bubble's superview's coordinate system.
// `bubbleAlignmentOffset` is the distance from the leading edge of the bubble
// to the anchor point if leading aligned, and from the trailing edge of the
// bubble to the anchor point if trailing aligned. `direction` is the bubble's
// direction. `alignment` is the bubble's alignment. `boundingSize` is the size
// of the superview. `isRTL` is `true` if the coordinates are in right-to-left
// language coordinates and `false` otherwise. This method is unit tested so it
// cannot be in the above anonymous namespace.
CGSize BubbleMaxSize(CGPoint anchorPoint,
                     CGFloat bubbleAlignmentOffset,
                     BubbleArrowDirection direction,
                     BubbleAlignment alignment,
                     CGSize boundingSize,
                     bool isRTL) {
  CGFloat maxWidth = BubbleMaxWidth(anchorPoint.x, bubbleAlignmentOffset,
                                    alignment, boundingSize.width, isRTL);
  CGFloat maxHeight =
      BubbleMaxHeight(anchorPoint.y, direction, boundingSize.height);
  return CGSizeMake(maxWidth, maxHeight);
}

CGSize BubbleMaxSize(CGPoint anchorPoint,
                     CGFloat bubbleAlignmentOffset,
                     BubbleArrowDirection direction,
                     BubbleAlignment alignment,
                     CGSize boundingSize) {
  bool isRTL = base::i18n::IsRTL();
  return BubbleMaxSize(anchorPoint, bubbleAlignmentOffset, direction, alignment,
                       boundingSize, isRTL);
}

// Calculate the bubble's frame. `anchorPoint` is the point on the UI element
// the bubble is pointing to. `bubbleAlignmentOffset` is the distance from the
// leading edge of the bubble to the anchor point if leading aligned, and from
// the trailing edge of the bubble to the anchor point if trailing aligned.
// `size` is the size of the bubble. `direction` is the direction the bubble's
// arrow is pointing. `alignment` is the alignment of the anchor (either
// leading, centered, or trailing). `boundingWidth` is the width of the bubble's
// superview. `isRTL` is `true` if the coordinates are in right-to-left language
// coordinates and `false` otherwise. This method is unit tested so it cannot be
// in the above anonymous namespace.
CGRect BubbleFrame(CGPoint anchorPoint,
                   CGFloat bubbleAlignmentOffset,
                   CGSize size,
                   BubbleArrowDirection direction,
                   BubbleAlignment alignment,
                   CGFloat boundingWidth,
                   bool isRTL) {
  CGFloat leading =
      LeadingDistance(anchorPoint, bubbleAlignmentOffset, alignment, size.width,
                      boundingWidth, isRTL);
  CGFloat originY = OriginY(anchorPoint, direction, size.height);
  // Use a `LayoutRect` to ensure that the bubble is mirrored in RTL contexts.
  base::i18n::TextDirection textDirection =
      isRTL ? base::i18n::RIGHT_TO_LEFT : base::i18n::LEFT_TO_RIGHT;
  CGRect bubbleFrame = LayoutRectGetRectUsingDirection(
      LayoutRectMake(leading, boundingWidth, originY, size.width, size.height),
      textDirection);
  return bubbleFrame;
}

CGRect BubbleFrame(CGPoint anchorPoint,
                   CGFloat bubbleAlignmentOffset,
                   CGSize size,
                   BubbleArrowDirection direction,
                   BubbleAlignment alignment,
                   CGFloat boundingWidth) {
  bool isRTL = base::i18n::IsRTL();
  return BubbleFrame(anchorPoint, bubbleAlignmentOffset, size, direction,
                     alignment, boundingWidth, isRTL);
}

CGFloat FloatingArrowAlignmentOffset(CGFloat boundingWidth,
                                     CGPoint anchorPoint,
                                     BubbleAlignment alignment) {
  CGFloat alignmentOffset;
  BOOL isRTL = base::i18n::IsRTL();
  switch (alignment) {
    case BubbleAlignmentLeading:
      alignmentOffset = isRTL ? boundingWidth - anchorPoint.x : anchorPoint.x;
      break;
    case BubbleAlignmentCenter:
      alignmentOffset = 0.0f;  // value is ignored when laying out the arrow.
      break;
    case BubbleAlignmentTrailing:
      alignmentOffset = isRTL ? anchorPoint.x : boundingWidth - anchorPoint.x;
      break;
  }
  // Alignment offset must be greater than `BubbleDefaultAlignmentOffset` to
  // make sure the arrow is in the frame of the background of the bubble. The
  // maximum is set to the middle of the bubble so the arrow stays close to the
  // leading edge when using a leading alignment.
  return MAX(MIN(kBubbleMaxWidth / 2, alignmentOffset),
             BubbleDefaultAlignmentOffset());
}

}  // namespace bubble_util
