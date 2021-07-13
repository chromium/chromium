// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_UTIL_UI_UTIL_H_
#define IOS_CHROME_BROWSER_UI_UTIL_UI_UTIL_H_

#include <CoreGraphics/CoreGraphics.h>

// UI Util containing functions that do not require Objective-C.

// Running on an iPad?
// TODO(crbug.com/330493): deprecated, use GetDeviceFormFactor() from
// ui/base/device_form_factor.h instead.
bool IsIPadIdiom();

// Returns the height of the screen in the current orientation.
CGFloat CurrentScreenHeight();

// Returns the width of the screen in the current orientation.
CGFloat CurrentScreenWidth();

// Returns true if the device is an iPhone X.
bool IsIPhoneX();

// Returns true if the device is considered as a small device.
bool IsSmallDevice();

// Returns the approximate corner radius of the current device.
CGFloat DeviceCornerRadius();

// Returns the closest pixel-aligned value less than |value|, taking the scale
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

// Returns a square CGRect centered at |x|, |y| with a width of |width|.
// Both the position and the size of the CGRect will be aligned to points.
CGRect CGRectMakeAlignedAndCenteredAt(CGFloat x, CGFloat y, CGFloat width);

// Returns a rectangle of size |rectSize| centered inside |frameSize|.
CGRect CGRectMakeCenteredRectInFrame(CGSize frameSize, CGSize rectSize);

// Returns whether |a| and |b| are within CGFloat's epsilon value.
bool AreCGFloatsEqual(CGFloat a, CGFloat b);

// This function is used to figure out how to resize an image from an
// |originalSize| to a |targetSize|. It returns a |revisedTargetSize| of the
// resized  image and |projectTo| that is used to describe the rectangle in the
// target that the image will be covering. Returned values are always floored to
// integral values.
//
// The ProjectionMode describes in which way the stretching will apply.
//
enum class ProjectionMode {
  // Just stretches the source into the destination, not preserving aspect ratio
  // at all.
  // |projectTo| and |revisedTargetSize| will be set to |targetSize|
  kFill,

  // Scale to the target, maintaining aspect ratio, clipping the excess, while
  // keeping the image centered.
  // Large original sizes are shrunk until they fit on one side, small original
  // sizes are expanded.
  // |projectTo| will be a subset of |originalSize|
  // |revisedTargetSize| will be set to |targetSize|
  kAspectFill,

  // Same as kAspectFill, except that the bottom part of the image will be
  // clipped. The image will still be horizontally centered.
  kAspectFillAlignTop,

  // Fit the image in the target so it fits completely inside, preserving aspect
  // ratio. This will leave bands with with no data in the target.
  // |projectTo| will be set to |originalSize|
  // |revisedTargetSize| will be a smaller in one direction from |targetSize|
  kAspectFit,

  // Scale to the target, maintaining aspect ratio and not clipping the excess.
  // |projectTo| will be set to |originalSize|
  // |revisedTargetSize| will be a larger in one direction from |targetSize|
  kAspectFillNoClipping,
};
void CalculateProjection(CGSize originalSize,
                         CGSize targetSize,
                         ProjectionMode projectionMode,
                         CGSize& revisedTargetSize,
                         CGRect& projectTo);

#endif  // IOS_CHROME_BROWSER_UI_UTIL_UI_UTIL_H_
