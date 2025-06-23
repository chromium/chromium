// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/ui_util/dynamic_type_util.h"

#import "base/metrics/histogram_macros.h"

namespace ui_util {

float SystemSuggestedFontSizeMultiplier() {
  return SystemSuggestedFontSizeMultiplier(
      UIApplication.sharedApplication.preferredContentSizeCategory);
}

float SystemSuggestedFontSizeMultiplier(UIContentSizeCategory category) {
  // Scaling numbers are calculated by [UIFont
  // preferredFontForTextStyle:UIFontTextStyleBody].pointSize, which are [14,
  // 15, 16, 17(default), 19, 21, 23, 28, 33, 40, 47, 53].
  static NSDictionary* font_size_map = @{
    UIContentSizeCategoryUnspecified : @1,
    UIContentSizeCategoryExtraSmall : @0.82,
    UIContentSizeCategorySmall : @0.88,
    UIContentSizeCategoryMedium : @0.94,
    UIContentSizeCategoryLarge : @1,  // system default
    UIContentSizeCategoryExtraLarge : @1.12,
    UIContentSizeCategoryExtraExtraLarge : @1.24,
    UIContentSizeCategoryExtraExtraExtraLarge : @1.35,
    UIContentSizeCategoryAccessibilityMedium : @1.65,
    UIContentSizeCategoryAccessibilityLarge : @1.94,
    UIContentSizeCategoryAccessibilityExtraLarge : @2.35,
    UIContentSizeCategoryAccessibilityExtraExtraLarge : @2.76,
    UIContentSizeCategoryAccessibilityExtraExtraExtraLarge : @3.12,
  };
  NSNumber* font_size = font_size_map[category];
  static dispatch_once_t once_token;
  dispatch_once(&once_token, ^{
    // In case there is a new accessibility value, log if there is a value we
    // are missing. Use the sharedApplication value as this method can be called
    // with an explicit value for the first time.
    UMA_HISTOGRAM_BOOLEAN("Accessibility.iOS.NewLargerTextCategory",
                          !font_size_map[UIApplication.sharedApplication
                                             .preferredContentSizeCategory]);
  });
  return font_size ? font_size.floatValue : 1;
}

float SystemSuggestedFontSizeMultiplier(UIContentSizeCategory category,
                                        UIContentSizeCategory min_category,
                                        UIContentSizeCategory max_category) {
  float min_multiplier = SystemSuggestedFontSizeMultiplier(min_category);
  float max_multiplier = SystemSuggestedFontSizeMultiplier(max_category);
  DCHECK(min_multiplier < max_multiplier);
  return std::min(
      max_multiplier,
      std::max(min_multiplier, SystemSuggestedFontSizeMultiplier(category)));
}

}  // namespace ui_util
