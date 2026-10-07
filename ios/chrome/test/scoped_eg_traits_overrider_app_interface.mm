// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/scoped_eg_traits_overrider_app_interface.h"

@implementation ScopedTraitOverriderAppInterface

+ (UIContentSizeCategory)contentSizeCategoryOverrideForViewController:
    (UIViewController*)viewController {
  id<UITraitOverrides> traitOverrides = viewController.traitOverrides;
  if (![traitOverrides
          containsTrait:UITraitPreferredContentSizeCategory.class]) {
    return nil;
  }
  return traitOverrides.preferredContentSizeCategory;
}

+ (void)setContentSizeCategoryOverride:(UIContentSizeCategory)category
                     forViewController:(UIViewController*)viewController {
  id<UITraitOverrides> traitOverrides = viewController.traitOverrides;
  if (!category) {
    [traitOverrides removeTrait:UITraitPreferredContentSizeCategory.class];
    return;
  }
  traitOverrides.preferredContentSizeCategory = category;
}

@end
