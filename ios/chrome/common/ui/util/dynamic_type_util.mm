// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/util/dynamic_type_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

UIFont* PreferredFontForTextStyleWithMaxCategory(
    UIFontTextStyle style,
    UIContentSizeCategory currentCategory,
    UIContentSizeCategory maxCategory) {
  NSComparisonResult result =
      UIContentSizeCategoryCompareToCategory(currentCategory, maxCategory);
  UIContentSizeCategory category =
      result == NSOrderedDescending ? maxCategory : currentCategory;
  return [UIFont preferredFontForTextStyle:style
             compatibleWithTraitCollection:
                 [UITraitCollection
                     traitCollectionWithPreferredContentSizeCategory:category]];
}
