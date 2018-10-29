// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/background_generator.h"

#import <QuartzCore/QuartzCore.h>
#include <stddef.h>

#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#import "ios/chrome/browser/ui/util/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// This is a utility function that may be used as a standalone helper function
// to generate a radial gradient UIImage.
UIImage* GetRadialGradient(CGRect backgroundRect,
                           CGPoint centerPoint,
                           CGFloat radius,
                           CGFloat centerColor,
                           CGFloat outsideColor,
                           UIImage* tileImage,
                           UIImage* logoImage) {
  UIGraphicsBeginImageContextWithOptions(backgroundRect.size, YES, 0);
  CGContextRef context = UIGraphicsGetCurrentContext();
  CGFloat gradient_colors[4] = {centerColor, 1.0, outsideColor, 1.0};
  const size_t kColorCount = 2;
  base::ScopedCFTypeRef<CGColorSpaceRef> grey_space(
      CGColorSpaceCreateDeviceGray());
  DCHECK_EQ(2u, CGColorSpaceGetNumberOfComponents(grey_space));
  base::ScopedCFTypeRef<CGGradientRef> gradient(
      CGGradientCreateWithColorComponents(grey_space, gradient_colors, nullptr,
                                          kColorCount));
  CGContextDrawRadialGradient(context, gradient, centerPoint, 0, centerPoint,
                              radius, kCGGradientDrawsAfterEndLocation);
  if (tileImage)
    [tileImage drawAsPatternInRect:backgroundRect];
  if (logoImage) {
    CGPoint corner = AlignPointToPixel(
        CGPointMake(centerPoint.x - logoImage.size.width / 2.0,
                    centerPoint.y - logoImage.size.height / 2.0));
    [logoImage drawAtPoint:corner];
  }
  UIImage* background = UIGraphicsGetImageFromCurrentImageContext();
  UIGraphicsEndImageContext();
  return background;
}
