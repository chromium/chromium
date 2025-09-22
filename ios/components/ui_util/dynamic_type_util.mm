// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/ui_util/dynamic_type_util.h"

#import "base/metrics/histogram_functions.h"
#import "build/config/ios/buildflags.h"

namespace {

// Returns the preferred content size category from the system.
UIContentSizeCategory SystemPreferredContentSizeCategory() {
#if BUILDFLAG(IOS_IS_APP_EXTENSION)
  // App extensions can not access -sharedApplication, so fall back to using
  // UIScreen's -mainScreen to get the preferred content size category.
  return UIScreen.mainScreen.traitCollection.preferredContentSizeCategory;
#else
  return UIApplication.sharedApplication.preferredContentSizeCategory;
#endif
}

// Structure that describes a content size category.
typedef struct {
  const ui_util::IOSContentSizeCategory category;
  const float multiplier;
} ContentSizeCategoryDescription;

// Value for the unspecified ContentSizeCategoryDescription.
constexpr ContentSizeCategoryDescription kUnspecifiedContentSizeCategory = {
    ui_util::IOSContentSizeCategory::kUnspecified,
    /*multiplier=*/1.0,
};

// Returns a ContentSizeCategoryDescription struct based on `category` if valid.
std::optional<ContentSizeCategoryDescription> GetContentSizeCategoryDescription(
    UIContentSizeCategory category) {
  if ([category isEqual:UIContentSizeCategoryUnspecified]) {
    return kUnspecifiedContentSizeCategory;
  }
  if ([category isEqual:UIContentSizeCategoryExtraSmall]) {
    return ContentSizeCategoryDescription(
        ui_util::IOSContentSizeCategory::kExtraSmall, /*multiplier=*/0.82);
  }
  if ([category isEqual:UIContentSizeCategorySmall]) {
    return ContentSizeCategoryDescription(
        ui_util::IOSContentSizeCategory::kSmall, /*multiplier=*/0.88);
  }
  if ([category isEqual:UIContentSizeCategoryMedium]) {
    return ContentSizeCategoryDescription(
        ui_util::IOSContentSizeCategory::kMedium, /*multiplier=*/0.94);
  }
  if ([category isEqual:UIContentSizeCategoryLarge]) {
    return ContentSizeCategoryDescription(
        ui_util::IOSContentSizeCategory::kLarge, /*multiplier=*/1);
  }
  if ([category isEqual:UIContentSizeCategoryExtraLarge]) {
    return ContentSizeCategoryDescription(
        ui_util::IOSContentSizeCategory::kExtraLarge, /*multiplier=*/1.12);
  }
  if ([category isEqual:UIContentSizeCategoryExtraExtraLarge]) {
    return ContentSizeCategoryDescription(
        ui_util::IOSContentSizeCategory::kExtraExtraLarge, /*multiplier=*/1.24);
  }
  if ([category isEqual:UIContentSizeCategoryExtraExtraExtraLarge]) {
    return ContentSizeCategoryDescription(
        ui_util::IOSContentSizeCategory::kExtraExtraExtraLarge,
        /*multiplier=*/1.35);
  }
  if ([category isEqual:UIContentSizeCategoryAccessibilityMedium]) {
    return ContentSizeCategoryDescription(
        ui_util::IOSContentSizeCategory::kAccessibilityMedium,
        /*multiplier=*/1.65);
  }
  if ([category isEqual:UIContentSizeCategoryAccessibilityLarge]) {
    return ContentSizeCategoryDescription(
        ui_util::IOSContentSizeCategory::kAccessibilityLarge,
        /*multiplier=*/1.94);
  }
  if ([category isEqual:UIContentSizeCategoryAccessibilityExtraLarge]) {
    return ContentSizeCategoryDescription(
        ui_util::IOSContentSizeCategory::kAccessibilityExtraLarge,
        /*multiplier=*/2.35);
  }
  if ([category isEqual:UIContentSizeCategoryAccessibilityExtraExtraLarge]) {
    return ContentSizeCategoryDescription(
        ui_util::IOSContentSizeCategory::kAccessibilityExtraExtraLarge,
        /*multiplier=*/2.76);
  }
  if ([category
          isEqual:UIContentSizeCategoryAccessibilityExtraExtraExtraLarge]) {
    return ContentSizeCategoryDescription(
        ui_util::IOSContentSizeCategory::kAccessibilityExtraExtraExtraLarge,
        /*multiplier=*/3.12);
  }
  return std::nullopt;
}

}  // namespace

namespace ui_util {

ui_util::IOSContentSizeCategory GetPreferredContentSizeCategory() {
  std::optional<ContentSizeCategoryDescription>
      preferred_system_font_decription = GetContentSizeCategoryDescription(
          SystemPreferredContentSizeCategory());
  return preferred_system_font_decription
      .value_or(kUnspecifiedContentSizeCategory)
      .category;
}

void RecordSystemFontSizeMetrics() {
  std::optional<ContentSizeCategoryDescription>
      preferred_system_font_decription = GetContentSizeCategoryDescription(
          SystemPreferredContentSizeCategory());
  bool has_value = preferred_system_font_decription.has_value();
  // In case there is a new accessibility value, log if there is a value we
  // are missing. Use the sharedApplication value as this method can be called
  // with an explicit value for the first time.
  base::UmaHistogramBoolean("Accessibility.iOS.NewLargerTextCategory",
                            !has_value);
  if (has_value) {
    base::UmaHistogramEnumeration(
        "IOS.System.PreferredSystemFontSize",
        preferred_system_font_decription.value().category);
  }
}

float SystemSuggestedFontSizeMultiplier() {
  return SystemSuggestedFontSizeMultiplier(
      SystemPreferredContentSizeCategory());
}

float SystemSuggestedFontSizeMultiplier(UIContentSizeCategory category) {
  // Scaling numbers are calculated by [UIFont
  // preferredFontForTextStyle:UIFontTextStyleBody].pointSize, which are [14,
  // 15, 16, 17(default), 19, 21, 23, 28, 33, 40, 47, 53].
  std::optional<ContentSizeCategoryDescription> system_font_description =
      GetContentSizeCategoryDescription(category);
  return system_font_description.value_or(kUnspecifiedContentSizeCategory)
      .multiplier;
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
