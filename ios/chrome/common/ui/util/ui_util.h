// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_UTIL_UI_UTIL_H_
#define IOS_CHROME_COMMON_UI_UTIL_UI_UTIL_H_

#include <UIKit/UIKit.h>

// UI Util containing functions that do not require Objective-C.

// Returns the height of the screen in the current orientation.
CGFloat CurrentScreenHeight();

// Returns the width of the screen in the current orientation.
CGFloat CurrentScreenWidth();

// Returns the closest pixel-aligned value less than `value`, taking the scale
// factor into account. At a scale of 1, equivalent to floor().
CGFloat AlignValueToPixel(CGFloat value);

// Returns the point resulting from applying AlignValueToPixel() to both
// components.
CGPoint AlignPointToPixel(CGPoint point);

// Returns the rectangle resulting from applying AlignPointToPixel() to the
// origin.
CGRect AlignRectToPixel(CGRect rect);

// Returns the rectangle resulting from applying AlignPointToPixel() to the
// origin, and ui::AlignSizeToUpperPixel() to the size.
CGRect AlignRectOriginAndSizeToPixels(CGRect rect);

// Returns a square CGRect centered at `x`, `y` with a width of `width`.
// Both the position and the size of the CGRect will be aligned to points.
CGRect CGRectMakeAlignedAndCenteredAt(CGFloat x, CGFloat y, CGFloat width);

// Returns a rectangle of size `rectSize` centered inside `frameSize`.
CGRect CGRectMakeCenteredRectInFrame(CGSize frameSize, CGSize rectSize);

// Returns whether `a` and `b` are within CGFloat's epsilon value.
bool AreCGFloatsEqual(CGFloat a, CGFloat b);

// Whether the `environment` has a regular vertical and regular horizontal
// size class.
bool IsRegularXRegularSizeClass(id<UITraitEnvironment> environment);

// Whether the `traitCollection` has a regular vertical and regular horizontal
// size class.
bool IsRegularXRegularSizeClass(UITraitCollection* traitCollection);

#endif  // IOS_CHROME_COMMON_UI_UTIL_UI_UTIL_H_
