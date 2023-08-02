// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/util/ui_util.h"

#import <UIKit/UIKit.h>
#import <cmath>
#import <limits>

#import "base/mac/foundation_util.h"
#import "ui/gfx/ios/uikit_util.h"

CGFloat DeviceCornerRadius() {
  UIUserInterfaceIdiom idiom = [[UIDevice currentDevice] userInterfaceIdiom];

  UIWindow* window = nil;
  for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
    UIWindowScene* windowScene =
        base::mac::ObjCCastStrict<UIWindowScene>(scene);
    UIWindow* firstWindow = [windowScene.windows firstObject];
    if (firstWindow) {
      window = firstWindow;
      break;
    }
  }

  const BOOL isRoundedDevice =
      (idiom == UIUserInterfaceIdiomPhone && window.safeAreaInsets.bottom);
  return isRoundedDevice ? 40.0 : 0.0;
}

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
