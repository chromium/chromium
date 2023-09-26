// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bubble/bubble_util.h"

#import <ostream>

#import "base/check_op.h"
#import "base/i18n/rtl.h"
#import "base/notreached.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/ui/bubble/bubble_constants.h"

namespace {

// Bubble's maximum width, preserves readability, ensuring that the bubble does
// not span across wide screens.
const CGFloat kBubbleMaxWidth = 375.0f;

// Whether bubble with arrow direction `direction` is pointing left.
BOOL IsArrowPointingLeft(BubbleArrowDirection direction, bool isRTL) {
  return direction ==
         (isRTL ? BubbleArrowDirectionTrailing : BubbleArrowDirectionLeading);
}

// Calculate the distance from the bubble's leading edge to the leading edge of
// its bounding coordinate system. In LTR contexts, the returned float is the
// x-coordinate of the bubble's origin. This calculation is based on
// `anchorPoint`, which is the point of the target UI element the bubble is
// anchored at, and the bubble's alignment offset, alignment, direction, and
// size. The returned float is in the same coordinate system as `anchorPoint`,
// which should be the coordinate system in which the bubble is drawn.
CGFloat LeadingDistance(CGPoint anchorPoint,
                        BubbleArrowDirection arrowDirection,
                        CGFloat bubbleAlignmentOffset,
                        BubbleAlignment alignment,
                        CGFloat bubbleWidth,
                        CGFloat boundingWidth,
                        bool isRTL) {
  // Find `leadingOffset`, the distance from the bubble's leading edge to the
  // anchor point. This depends on alignment and bubble width.
  CGFloat leadingOffset;
  switch (arrowDirection) {
    case BubbleArrowDirectionUp:
    case BubbleArrowDirectionDown:
      switch (alignment) {
        case BubbleAlignmentTopOrLeading:
          leadingOffset = bubbleAlignmentOffset;
          break;
        case BubbleAlignmentCenter:
          leadingOffset = bubbleWidth / 2.0f;
          break;
        case BubbleAlignmentBottomOrTrailing:
          leadingOffset = bubbleWidth - bubbleAlignmentOffset;
          break;
        default:
          NOTREACHED() << "Invalid bubble alignment " << alignment;
          break;
      }
      break;
    case BubbleArrowDirectionLeading:
      leadingOffset = 0;
      break;
    case BubbleArrowDirectionTrailing:
      leadingOffset = bubbleWidth;
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
// point of the UI element the bubble is anchored at, and the bubble's alignment
// offset, alignment, direction and size. The returned float is in the same
// coordinate system as `anchorPoint`, which should be the coordinate system in
// which the bubble is drawn.
CGFloat OriginY(CGPoint anchorPoint,
                BubbleArrowDirection arrowDirection,
                CGFloat bubbleAlignmentOffset,
                BubbleAlignment alignment,
                CGFloat bubbleHeight) {
  CGFloat originY;
  switch (arrowDirection) {
    case BubbleArrowDirectionUp:
      originY = anchorPoint.y;
      break;
    case BubbleArrowDirectionDown:
      originY = anchorPoint.y - bubbleHeight;
      break;
    case BubbleArrowDirectionLeading:
    case BubbleArrowDirectionTrailing:
      switch (alignment) {
        case BubbleAlignmentTopOrLeading:
          originY = anchorPoint.y - bubbleAlignmentOffset;
          break;
        case BubbleAlignmentCenter:
          originY = anchorPoint.y - bubbleHeight / 2;
          break;
        case BubbleAlignmentBottomOrTrailing:
          originY = anchorPoint.y - (bubbleHeight - bubbleAlignmentOffset);
          break;
      }
      break;
  }
  // Round down the origin Y.
  return floor(originY);
}

// Calculate the maximum width of the bubble such that it stays within its
// bounding coordinate space. `anchorPointX` is the x-coordinate of the point on
// the target UI element the bubble is anchored at. It is in the coordinate
// system in which the bubble is drawn. `direction` is the direction the bubble
// arrow points to. `bubbleAlignmentOffset` is the distance from the leading
// edge of the bubble to the anchor point if leading aligned, and from the
// trailing edge of the bubble to the anchor point if trailing aligned.
// `alignment` is the bubble's alignment, `boundingWidth` is the width of the
// coordinate space in which the bubble is drawn, and `isRTL` is true if the
// language is RTL and `false` otherwise.
CGFloat BubbleMaxWidth(CGFloat anchorPointX,
                       BubbleArrowDirection direction,
                       CGFloat bubbleAlignmentOffset,
                       BubbleAlignment alignment,
                       CGFloat boundingWidth,
                       bool isRTL) {
  CGFloat maxWidth;
  // Space on the left of the anchor point.
  CGFloat distanceToLeftEdge = anchorPointX;
  // Space on the right of the anchor point.
  CGFloat distanceToRightEdge = boundingWidth - anchorPointX;
  switch (direction) {
    case BubbleArrowDirectionUp:
    case BubbleArrowDirectionDown:
      switch (alignment) {
        case BubbleAlignmentTopOrLeading:
          // The bubble can use the space from the anchor point to the trailing
          // edge.
          maxWidth = (isRTL ? distanceToLeftEdge : distanceToRightEdge) +
                     bubbleAlignmentOffset;
          break;
        case BubbleAlignmentCenter:
          // The width of half the bubble cannot exceed the distance from the
          // anchor point to the closest edge of the superview.
          maxWidth = MIN(distanceToLeftEdge, distanceToRightEdge) * 2.0f;
          break;
        case BubbleAlignmentBottomOrTrailing:
          // The bubble can use the space from the anchor point to the leading
          // edge.
          maxWidth = (isRTL ? distanceToRightEdge : distanceToLeftEdge) +
                     bubbleAlignmentOffset;
          break;
        default:
          NOTREACHED() << "Invalid bubble alignment " << alignment;
          break;
      }
      break;
    case BubbleArrowDirectionLeading:
    case BubbleArrowDirectionTrailing:
      if (IsArrowPointingLeft(direction, isRTL)) {
        maxWidth = distanceToRightEdge;
      } else {
        maxWidth = distanceToLeftEdge;
      }
      break;
  }
  // Round up the width.
  return ceil(MIN(maxWidth, kBubbleMaxWidth));
}

// Calculate the maximum height of the bubble such that it stays within its
// bounding coordinate space. `anchorPointY` is the y-coordinate of the point on
// the target UI element the bubble is anchored at. It is in the coordinate
// system in which the bubble is drawn. `direction` is the direction the arrow
// is pointing. `bubbleAlignmentOffset` is the distance from the leading or top
// edge of the bubble to the anchor point if leading aligned, and from the
// bottom or trailing edge of the bubble to the anchor point if trailing
// aligned. `alignment` is the bubble's alignment, `boundingHeight` is the
// height of the coordinate space in which the bubble is drawn.
CGFloat BubbleMaxHeight(CGFloat anchorPointY,
                        BubbleArrowDirection direction,
                        CGFloat bubbleAlignmentOffset,
                        BubbleAlignment alignment,
                        CGFloat boundingHeight) {
  CGFloat maxHeight;
  // Space on the top of the anchor point.
  CGFloat distanceToTopEdge = anchorPointY;
  // Space on the bottom of the anchor point.
  CGFloat distanceToBottomEdge = boundingHeight - anchorPointY;
  switch (direction) {
    case BubbleArrowDirectionUp:
      maxHeight = distanceToBottomEdge;
      break;
    case BubbleArrowDirectionDown:
      maxHeight = distanceToTopEdge;
      break;
    case BubbleArrowDirectionLeading:
    case BubbleArrowDirectionTrailing:
      switch (alignment) {
        case BubbleAlignmentTopOrLeading:
          // The bubble can use the space from the anchor point to the bottom
          // edge.
          maxHeight = distanceToBottomEdge + bubbleAlignmentOffset;
          break;
        case BubbleAlignmentCenter:
          // The height of half the bubble cannot exceed the distance from the
          // anchor point to the closest edge of the superview.
          maxHeight = MIN(distanceToTopEdge, distanceToBottomEdge) * 2.0f;
          break;
        case BubbleAlignmentBottomOrTrailing:
          // The bubble can use the space from the anchor point to the top
          // edge.
          maxHeight = distanceToTopEdge + bubbleAlignmentOffset;
          break;
      }
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
  bool isRTL = base::i18n::IsRTL();
  switch (arrowDirection) {
    case BubbleArrowDirectionUp:
      anchorPoint.x = CGRectGetMidX(targetFrame);
      anchorPoint.y = CGRectGetMaxY(targetFrame);
      break;
    case BubbleArrowDirectionDown:
      anchorPoint.x = CGRectGetMidX(targetFrame);
      anchorPoint.y = CGRectGetMinY(targetFrame);
      break;
    case BubbleArrowDirectionLeading:
    case BubbleArrowDirectionTrailing:
      anchorPoint.x = IsArrowPointingLeft(arrowDirection, isRTL)
                          ? CGRectGetMaxX(targetFrame)
                          : CGRectGetMinX(targetFrame);
      anchorPoint.y = CGRectGetMidY(targetFrame);
      break;
  }
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
  CGFloat maxWidth =
      BubbleMaxWidth(anchorPoint.x, direction, bubbleAlignmentOffset, alignment,
                     boundingSize.width, isRTL);
  CGFloat maxHeight =
      BubbleMaxHeight(anchorPoint.y, direction, bubbleAlignmentOffset,
                      alignment, boundingSize.height);
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
      LeadingDistance(anchorPoint, direction, bubbleAlignmentOffset, alignment,
                      size.width, boundingWidth, isRTL);
  CGFloat originY = OriginY(anchorPoint, direction, bubbleAlignmentOffset,
                            alignment, size.height);
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
  // TODO(crbug.com/1467873): Leading and trailing direction.
  switch (alignment) {
    case BubbleAlignmentTopOrLeading:
      alignmentOffset = isRTL ? boundingWidth - anchorPoint.x : anchorPoint.x;
      break;
    case BubbleAlignmentCenter:
      alignmentOffset = 0.0f;  // value is ignored when laying out the arrow.
      break;
    case BubbleAlignmentBottomOrTrailing:
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
