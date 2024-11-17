// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/util/dynamic_type_util.h"

UIFont* PreferredFontForTextStyleWithMaxCategory(
    UIFontTextStyle style,
    UIContentSizeCategory currentCategory,
    UIContentSizeCategory maxCategory) {
  UIContentSizeCategory category =
      ContentSizeCategoryWithMaxCategory(currentCategory, maxCategory);
  return [UIFont preferredFontForTextStyle:style
             compatibleWithTraitCollection:
                 [UITraitCollection
                     traitCollectionWithPreferredContentSizeCategory:category]];
}

UIContentSizeCategory ContentSizeCategoryWithMaxCategory(
    UIContentSizeCategory currentCategory,
    UIContentSizeCategory maxCategory) {
  NSComparisonResult result =
      UIContentSizeCategoryCompareToCategory(currentCategory, maxCategory);
  return result == NSOrderedDescending ? maxCategory : currentCategory;
}
