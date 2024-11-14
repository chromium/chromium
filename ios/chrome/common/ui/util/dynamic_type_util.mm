// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/util/dynamic_type_util.h"

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
