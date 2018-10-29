// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_UTIL_RTL_GEOMETRY_H_
#define IOS_CHROME_BROWSER_UI_UTIL_RTL_GEOMETRY_H_

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
// |leading| is the distance from the leading edge at which the resulting rect
// should be laid out; in LTR this will be the x-origin, in RTL it will be used
// to compute the x-origin.  |originY| is used to position the rect vertically.
struct LayoutRectPosition {
  CGFloat leading;
  CGFloat originY;
};

// The null LayoutRectPosition, with |leading| and |originY| equal to 0.0.
extern const LayoutRectPosition LayoutRectPositionZero;

// Returns a new LayoutRectPosition with the passed-in values.
LayoutRectPosition LayoutRectPositionMake(CGFloat leading, CGFloat originY);

// Returns YES if |a|'s values are equal to those of |b|.
BOOL LayoutRectPositionEqualToPosition(LayoutRectPosition a,
                                       LayoutRectPosition b);

// Returns a new LayoutRectPosition created by aligning |position|'s values to
// the nearest pixel boundary.
LayoutRectPosition AlignLayoutRectPositionToPixel(LayoutRectPosition position);

// A LayoutRect contains the information needed to generate a CGRect that may or
// may not be flipped if positioned in RTL or LTR contexts. |boundingWidth| is
// the width of the bounding coordinate space in which the resulting rect will
// be used.  |position| is used to describe the location of the resulting frame,
// and |size| is the size of resulting frame.
struct LayoutRect {
  CGFloat boundingWidth;
  LayoutRectPosition position;
  CGSize size;
};

// The null LayoutRect, with leading, boundingWidth and originY of 0.0, and
// a size of CGSizeZero.
extern const LayoutRect LayoutRectZero;

// Returns a new LayoutRect; |height| and |width| are used to construct the
// |size| field.
LayoutRect LayoutRectMake(CGFloat leading,
                          CGFloat boundingWidth,
                          CGFloat originY,
                          CGFloat width,
                          CGFloat height);

// Returns YES if |a|'s values are equal to those of |b|.
BOOL LayoutRectEqualToRect(LayoutRect a, LayoutRect b);

// Given |layout|, returns the rect for that layout in text direction
// |direction|.
CGRect LayoutRectGetRectUsingDirection(LayoutRect layout,
                                       base::i18n::TextDirection direction);
// As above, using |direction| == RIGHT_TO_LEFT if UseRTLLayout(), LEFT_TO_RIGHT
// otherwise.
CGRect LayoutRectGetRect(LayoutRect layout);

// Utilities for getting CALayer positioning values from a layoutRect.
// Given |layout|, return the bounds rectangle of the generated rect -- that is,
// a rect with origin (0,0) and size equal to |layout|'s size.
CGRect LayoutRectGetBoundsRect(LayoutRect layout);

// Given |layout| and some anchor point |anchor| (defined in the way that
// CALayer's anchorPoint property is), return the CGPoint that defines the
// position of a rect in the context used by |layout|.
CGPoint LayoutRectGetPositionForAnchorUsingDirection(
    LayoutRect layout,
    CGPoint anchor,
    base::i18n::TextDirection direction);

// As above, using |direction| == RIGHT_TO_LEFT if UseRTLLayout(), LEFT_TO_RIGHT
// otherwise.
CGPoint LayoutRectGetPositionForAnchor(LayoutRect layout, CGPoint anchor);

// Given |rect|, a rect, and |boundingRect|, a rect whose bounds are the
// context in which |rect|'s frame is interpreted, return the layout that
// defines |rect|, assuming |direction| is the direction |rect| was positioned
// under.
LayoutRect LayoutRectForRectInBoundingRectUsingDirection(
    CGRect rect,
    CGRect boundingRect,
    base::i18n::TextDirection direction);

// As above, using |direction| == RIGHT_TO_LEFT if UseRTLLayout(), LEFT_TO_RIGHT
// otherwise.
LayoutRect LayoutRectForRectInBoundingRect(CGRect rect, CGRect boundingRect);

// Given a layout |layout|, return the layout that defines the leading area up
// to |layout|.
LayoutRect LayoutRectGetLeadingLayout(LayoutRect layout);

// Given a layout |layout|, return the layout that defines the trailing area
// after |layout|.
LayoutRect LayoutRectGetTrailingLayout(LayoutRect layout);

// Return the trailing extent of |layout| (its leading plus its width).
CGFloat LayoutRectGetTrailingEdge(LayoutRect layout);

// A LayoutOffset is an x-offset specified in leading pixels.
typedef CGFloat LayoutOffset;

// Returns |point| with its x-value shifted |offset| pixels in the leading
// direction according to |direction|
CGPoint CGPointLayoutOffsetUsingDirection(CGPoint point,
                                          LayoutOffset offset,
                                          base::i18n::TextDirection direction);

// As above, using |direction| == RIGHT_TO_LEFT if UseRTLLayout(), LEFT_TO_RIGHT
// otherwise.
CGPoint CGPointLayoutOffset(CGPoint point, LayoutOffset offset);

// Returns |rect| with its x-origin shifted |offset| pixels in the leading
// direction according to |direction|
CGRect CGRectLayoutOffsetUsingDirection(CGRect rect,
                                        LayoutOffset offset,
                                        base::i18n::TextDirection direction);

// As above, using |direction| == RIGHT_TO_LEFT if UseRTLLayout(), LEFT_TO_RIGHT
// otherwise.
CGRect CGRectLayoutOffset(CGRect rect, LayoutOffset offset);

// Returns the leading offset of |rect| inside |boundingBox|, as a LayoutOffset.
LayoutOffset CGRectGetLeadingLayoutOffsetInBoundingRect(CGRect rect,
                                                        CGRect boundingRect);

// Returns the trailing offset of |rect| inside |boundingBox|, as a
// LayoutOffset. Note that this will be the distance from the trailing edge of
// |rect| to the trailing edge of |boundingRect|.
LayoutOffset CGRectGetTrailingLayoutOffsetInBoundingRect(CGRect rect,
                                                         CGRect boundingRect);

// Returns the leading content offset of |scrollView|.
LayoutOffset LeadingContentOffsetForScrollView(UIScrollView* scrollView);

// Utilities for mapping UIKit geometric structures to RTL-independent geometry.

// Get leading and trailing edges of |rect|, assuming layout direction
// |direction|.
CGFloat CGRectGetLeadingEdgeUsingDirection(CGRect rect,
                                           base::i18n::TextDirection direction);
CGFloat CGRectGetTrailingEdgeUsingDirection(
    CGRect rect,
    base::i18n::TextDirection direction);

// As above, with |direction| == LayoutDirection().
CGFloat CGRectGetLeadingEdge(CGRect rect);
CGFloat CGRectGetTrailingEdge(CGRect rect);

// Leading/trailing autoresizing masks. 'Leading' is 'Left' under iOS <= 8 or
// in an LTR language, 'Right' otherwise; 'Trailing' is the obverse.
UIViewAutoresizing UIViewAutoresizingFlexibleLeadingMargin();
UIViewAutoresizing UIViewAutoresizingFlexibleTrailingMargin();

// Text-direction aware UIEdgeInsets constructor; just like UIEdgeInsetsMake(),
// except |leading| and |trailing| map to left and right when |direction| is
// LEFT_TO_RIGHT, and are swapped for RIGHT_TO_LEFT.
UIEdgeInsets UIEdgeInsetsMakeUsingDirection(
    CGFloat top,
    CGFloat leading,
    CGFloat bottom,
    CGFloat trailing,
    base::i18n::TextDirection direction);
// As above, but uses LayoutDirection() for |direction|.
UIEdgeInsets UIEdgeInsetsMakeDirected(CGFloat top,
                                      CGFloat leading,
                                      CGFloat bottom,
                                      CGFloat trailing);

// Inverses of the above functions: return the leading/trailing inset for
// the current direction.
CGFloat UIEdgeInsetsGetLeading(UIEdgeInsets insets);
CGFloat UIEdgeInsetsGetTrailing(UIEdgeInsets insets);

#endif  // IOS_CHROME_BROWSER_UI_UTIL_RTL_GEOMETRY_H_
