// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/ui_util/dynamic_type_util.h"

#import "base/metrics/histogram_functions.h"
#import "ios/components/ui_util/content_size_category_description.h"

namespace {

NSDictionary* const kFontDescriptionDictionary = @{
  UIContentSizeCategoryUnspecified : [[ContentSizeCategoryDescription alloc]
      initWithCategory:ui_util::IOSContentSizeCategory::kUnspecified
            multiplier:1],
  UIContentSizeCategoryExtraSmall : [[ContentSizeCategoryDescription alloc]
      initWithCategory:ui_util::IOSContentSizeCategory::kExtraSmall
            multiplier:0.82],
  UIContentSizeCategorySmall : [[ContentSizeCategoryDescription alloc]
      initWithCategory:ui_util::IOSContentSizeCategory::kSmall
            multiplier:0.88],
  UIContentSizeCategoryMedium : [[ContentSizeCategoryDescription alloc]
      initWithCategory:ui_util::IOSContentSizeCategory::kMedium
            multiplier:0.94],
  UIContentSizeCategoryLarge : [[ContentSizeCategoryDescription alloc]
      initWithCategory:ui_util::IOSContentSizeCategory::kLarge
            multiplier:1],
  UIContentSizeCategoryExtraLarge : [[ContentSizeCategoryDescription alloc]
      initWithCategory:ui_util::IOSContentSizeCategory::kExtraLarge
            multiplier:1.12],
  UIContentSizeCategoryExtraExtraLarge : [[ContentSizeCategoryDescription alloc]
      initWithCategory:ui_util::IOSContentSizeCategory::kExtraExtraLarge
            multiplier:1.24],
  UIContentSizeCategoryExtraExtraExtraLarge : [[ContentSizeCategoryDescription
      alloc]
      initWithCategory:ui_util::IOSContentSizeCategory::kExtraExtraExtraLarge
            multiplier:1.35],
  UIContentSizeCategoryAccessibilityMedium :
      [[ContentSizeCategoryDescription alloc]
          initWithCategory:ui_util::IOSContentSizeCategory::kAccessibilityMedium
                multiplier:1.65],
  UIContentSizeCategoryAccessibilityLarge :
      [[ContentSizeCategoryDescription alloc]
          initWithCategory:ui_util::IOSContentSizeCategory::kAccessibilityLarge
                multiplier:1.94],
  UIContentSizeCategoryAccessibilityExtraLarge :
      [[ContentSizeCategoryDescription alloc]
          initWithCategory:ui_util::IOSContentSizeCategory::
                               kAccessibilityExtraLarge
                multiplier:2.35],
  UIContentSizeCategoryAccessibilityExtraExtraLarge :
      [[ContentSizeCategoryDescription alloc]
          initWithCategory:ui_util::IOSContentSizeCategory::
                               kAccessibilityExtraExtraLarge
                multiplier:2.76],
  UIContentSizeCategoryAccessibilityExtraExtraExtraLarge :
      [[ContentSizeCategoryDescription alloc]
          initWithCategory:ui_util::IOSContentSizeCategory::
                               kAccessibilityExtraExtraExtraLarge
                multiplier:3.12],
};

}  // namespace

namespace ui_util {

ui_util::IOSContentSizeCategory GetPreferredContentSizeCategory() {
  ContentSizeCategoryDescription* preferred_system_font_decription =
      kFontDescriptionDictionary[UIApplication.sharedApplication
                                     .preferredContentSizeCategory];
  return preferred_system_font_decription
             ? preferred_system_font_decription.category
             : IOSContentSizeCategory::kUnspecified;
}

void RecordSystemFontSizeMetrics() {
  ContentSizeCategoryDescription* preferred_system_font_decription =
      kFontDescriptionDictionary[UIApplication.sharedApplication
                                     .preferredContentSizeCategory];
  // In case there is a new accessibility value, log if there is a value we
  // are missing. Use the sharedApplication value as this method can be called
  // with an explicit value for the first time.
  base::UmaHistogramBoolean("Accessibility.iOS.NewLargerTextCategory",
                            !preferred_system_font_decription);
  base::UmaHistogramEnumeration("IOS.System.PreferredSystemFontSize",
                                preferred_system_font_decription.category);
}

float SystemSuggestedFontSizeMultiplier() {
  return SystemSuggestedFontSizeMultiplier(
      UIApplication.sharedApplication.preferredContentSizeCategory);
}

float SystemSuggestedFontSizeMultiplier(UIContentSizeCategory category) {
  // Scaling numbers are calculated by [UIFont
  // preferredFontForTextStyle:UIFontTextStyleBody].pointSize, which are [14,
  // 15, 16, 17(default), 19, 21, 23, 28, 33, 40, 47, 53].
  ContentSizeCategoryDescription* system_font_description =
      kFontDescriptionDictionary[category];
  return system_font_description ? system_font_description.multiplier : 1;
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
