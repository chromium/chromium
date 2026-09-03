// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_UTIL_RTL_GEOMETRY_H_
#define IOS_CHROME_BROWSER_SHARED_UI_UTIL_RTL_GEOMETRY_H_

#include <CoreGraphics/CoreGraphics.h>
#import <UIKit/UIKit.h>

#include "base/i18n/rtl.h"

// Utilities for direction-independent layout calculations and related
// functions.

// True if views should be laid out with full RTL mirroring.
bool UseRTLLayout();

// RIGHT_TO_LEFT if UseRTLLayout(), otherwise LEFT_TO_RIGHT.
base::i18n::TextDirection LayoutDirection();

// A LayoutRectPosition contains the information needed to position a CGRect,
// optionally flipping across its bounding coordinate space's midpoint Y axis.
// `leading` is the distance from the leading edge at which the resulting rect
// should be laid out; in LTR this will be the x-origin, in RTL it will be used
// to compute the x-origin.  `originY` is used to position the rect vertically.
struct LayoutRectPosition {
  CGFloat leading;
  CGFloat originY;
};

// Returns a new LayoutRectPosition with the passed-in values.
LayoutRectPosition LayoutRectPositionMake(CGFloat leading, CGFloat originY);

// A LayoutRect contains the information needed to generate a CGRect that may or
// may not be flipped if positioned in RTL or LTR contexts. `boundingWidth` is
// the width of the bounding coordinate space in which the resulting rect will
// be used.  `position` is used to describe the location of the resulting frame,
// and `size` is the size of resulting frame.
struct LayoutRect {
  CGFloat boundingWidth;
  LayoutRectPosition position;
  CGSize size;
};

// The null LayoutRect, with leading, boundingWidth and originY of 0.0, and
// a size of CGSizeZero.
extern const LayoutRect LayoutRectZero;

// Returns a new LayoutRect; `height` and `width` are used to construct the
// `size` field.
LayoutRect LayoutRectMake(CGFloat leading,
                          CGFloat boundingWidth,
                          CGFloat originY,
                          CGFloat width,
                          CGFloat height);

// Given `layout`, returns the rect for that layout in text direction
// `direction`.
CGRect LayoutRectGetRectUsingDirection(LayoutRect layout,
                                       base::i18n::TextDirection direction);
// As above, using `direction` == RIGHT_TO_LEFT if UseRTLLayout(), LEFT_TO_RIGHT
// otherwise.
CGRect LayoutRectGetRect(LayoutRect layout);

// Given `rect`, a rect, and `boundingRect`, a rect whose bounds are the
// context in which `rect`'s frame is interpreted, return the layout that
// defines `rect`, assuming `direction` is the direction `rect` was positioned
// under.
LayoutRect LayoutRectForRectInBoundingRectUsingDirection(
    CGRect rect,
    CGRect boundingRect,
    base::i18n::TextDirection direction);

// As above, using `direction` == RIGHT_TO_LEFT if UseRTLLayout(), LEFT_TO_RIGHT
// otherwise.
LayoutRect LayoutRectForRectInBoundingRect(CGRect rect, CGRect boundingRect);

// Inverses of UIEdgeInsetsMake: return the leading inset for the current
// direction.
CGFloat UIEdgeInsetsGetLeading(UIEdgeInsets insets);

// Utilities for testing RTL-dependent relations.

// YES if `a` is to the leading side of `b` given `direction`.
BOOL EdgeLeadsEdge(CGFloat a, CGFloat b, base::i18n::TextDirection direction);
// As above, `direction` == LayoutDirection().
BOOL EdgeLeadsEdge(CGFloat a, CGFloat b);

// Determines the best alignment for the provided `text`.
NSTextAlignment DetermineBestAlignmentForText(NSString* text);

#endif  // IOS_CHROME_BROWSER_SHARED_UI_UTIL_RTL_GEOMETRY_H_
