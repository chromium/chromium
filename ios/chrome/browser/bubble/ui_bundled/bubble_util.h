// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_BUBBLE_UTIL_H_
#define IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_BUBBLE_UTIL_H_

#import <Foundation/Foundation.h>

typedef NS_ENUM(NSInteger, BubbleAlignment);
typedef NS_ENUM(NSInteger, BubbleArrowDirection);

// TODO(crbug.com/40277360): Rename parameters.
namespace bubble_util {

// The default fixed distance from the leading edge of the bubble to the anchor
// point if leading aligned, and from the trailing edge of the bubble to the
// anchor point if trailing aligned.
CGFloat BubbleDefaultAlignmentOffset();

// Calculate the coordinates of the point of the bubble's arrow based on the
// `target_frame` of the target UI element and the bubble's `arrow_direction`.
// The returned point is in the same coordinate system as `target_frame`.
CGPoint AnchorPoint(CGRect target_frame, BubbleArrowDirection arrow_direction);

// Calculate the maximum size of the bubble such that it stays within its
// superview's bounding coordinate space and does not overlap the other side of
// the anchor point. `anchor_point` is the point on the target UI element the
// bubble is anchored at in the bubble's superview's coordinate system.
// `bubble_alignment_offset` is the distance from the leading edge of the bubble
// to the anchor point if leading aligned, and from the trailing edge of the
// bubble to the anchor point if trailing aligned. `direction` is the bubble's
// direction. `alignment` is the bubble's alignment. `bounding_size` is the size
// of the superview. Uses the ICU default locale of the device to determine
// whether the language is RTL.
CGSize BubbleMaxSize(CGPoint anchor_point,
                     CGFloat bubble_alignment_offset,
                     BubbleArrowDirection direction,
                     BubbleAlignment alignment,
                     CGSize bounding_size);

// Calculate the bubble's frame. `anchor_point` is the point on the UI element
// the bubble is pointing to. `bubble_alignment_offset` is the distance from the
// leading edge of the bubble to the anchor point if leading aligned, and from
// the trailing edge of the bubble to the anchor point if trailing aligned.
// `size` is the size of the bubble. `direction` is the direction the bubble's
// arrow is pointing. `alignment` is the alignment of the anchor (either
// leading, centered, or trailing). `bounding_width` is the width of the
// bubble's superview.
CGRect BubbleFrame(CGPoint anchor_point,
                   CGFloat bubble_alignment_offset,
                   CGSize size,
                   BubbleArrowDirection direction,
                   BubbleAlignment alignment,
                   CGFloat bounding_width);

// Returns alignment offset for a bubble with a floating arrow. `bounding_width`
// is the width of the bubble's superview. `anchor_point` is the point on the UI
// element the bubble is pointing to. `alignment` is the alignment of the anchor
// (either leading, centered, or trailing).
CGFloat FloatingArrowAlignmentOffset(CGFloat bounding_width,
                                     CGPoint anchor_point,
                                     BubbleAlignment alignment);
}  // namespace bubble_util

#endif  // IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_BUBBLE_UTIL_H_
