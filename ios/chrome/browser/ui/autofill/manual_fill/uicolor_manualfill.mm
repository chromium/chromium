// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/uicolor_manualfill.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation UIColor (ManualFill)

+ (UIColor*)cr_manualFillTintColor {
  static UIColor* color =
      [UIColor colorWithRed:0.10 green:0.45 blue:0.91 alpha:1.0];
  return color;
}

+ (UIColor*)cr_manualFillSeparatorColor {
  static UIColor* color = [UIColor colorWithRed:188 / 255.0
                                          green:187 / 255.0
                                           blue:193 / 255.0
                                          alpha:1 / 1.0];
  return color;
}

@end
