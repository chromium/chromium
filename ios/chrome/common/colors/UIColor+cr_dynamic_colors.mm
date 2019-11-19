// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/colors/UIColor+cr_dynamic_colors.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation UIColor (CRDynamicColors)

- (UIColor*)cr_resolvedColorWithTraitCollection:
    (UITraitCollection*)traitCollection {
  if (@available(iOS 13, *)) {
    return [self resolvedColorWithTraitCollection:traitCollection];
  }
  return self;
}

@end
