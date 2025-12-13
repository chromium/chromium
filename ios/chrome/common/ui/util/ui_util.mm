// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/util/ui_util.h"

#import <cmath>
#import <limits>

#import "base/apple/foundation_util.h"
#import "ui/gfx/ios/uikit_util.h"

CGFloat AlignValueToPixel(CGFloat value) {
  static CGFloat scale = [[UIScreen mainScreen] scale];
  return floor(value * scale) / scale;
}

CGPoint AlignPointToPixel(CGPoint point) {
  return CGPointMake(AlignValueToPixel(point.x), AlignValueToPixel(point.y));
}

CGRect AlignRectToPixel(CGRect rect) {
  rect.origin = AlignPointToPixel(rect.origin);
  return rect;
}

CGRect AlignRectOriginAndSizeToPixels(CGRect rect) {
  rect.origin = AlignPointToPixel(rect.origin);
  rect.size = ui::AlignSizeToUpperPixel(rect.size);
  return rect;
}

CGRect CGRectMakeAlignedAndCenteredAt(CGFloat x, CGFloat y, CGFloat width) {
  return AlignRectOriginAndSizeToPixels(
      CGRectMake(x - width / 2.0, y - width / 2.0, width, width));
}

CGRect CGRectMakeCenteredRectInFrame(CGSize frameSize, CGSize rectSize) {
  CGFloat rectX = AlignValueToPixel((frameSize.width - rectSize.width) / 2);
  CGFloat rectY = AlignValueToPixel((frameSize.height - rectSize.height) / 2);
  return CGRectMake(rectX, rectY, rectSize.width, rectSize.height);
}

bool AreCGFloatsEqual(CGFloat a, CGFloat b) {
  return std::fabs(a - b) <= std::numeric_limits<CGFloat>::epsilon();
}

bool IsRegularXRegularSizeClass(id<UITraitEnvironment> environment) {
  return IsRegularXRegularSizeClass(environment.traitCollection);
}

bool IsRegularXRegularSizeClass(UITraitCollection* traitCollection) {
  return traitCollection.verticalSizeClass == UIUserInterfaceSizeClassRegular &&
         traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassRegular;
}

UIColor* BlendColors(UIColor* color_1, UIColor* color_2, CGFloat fraction) {
  if (fraction <= 0.0) {
    return color_1;
  } else if (fraction >= 1.0) {
    return color_2;
  } else if ([color_1 isEqual:color_2]) {
    return color_1;
  }

  // Get RGBA components for the two colors, as inputs to the blend.
  CGFloat in_1[4];
  CGFloat in_2[4];
  [color_1 getRed:&in_1[0] green:&in_1[1] blue:&in_1[2] alpha:&in_1[3]];
  [color_2 getRed:&in_2[0] green:&in_2[1] blue:&in_2[2] alpha:&in_2[3]];

  // Blend each RGBA color component, based on the given fraction.
  CGFloat inverse = 1.0 - fraction;
  return [UIColor colorWithRed:inverse * in_1[0] + fraction * in_2[0]
                         green:inverse * in_1[1] + fraction * in_2[1]
                          blue:inverse * in_1[2] + fraction * in_2[2]
                         alpha:inverse * in_1[3] + fraction * in_2[3]];
}
