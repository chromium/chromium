// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_UI_UTIL_DYNAMIC_TYPE_UTIL_H_
#define IOS_COMPONENTS_UI_UTIL_DYNAMIC_TYPE_UTIL_H_

#import <UIKit/UIKit.h>

namespace ui_util {

// Content size category to report UMA metrics.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(IOSContentSizeCategory)
enum class IOSContentSizeCategory {
  kUnspecified = 0,
  kExtraSmall = 1,
  kSmall = 2,
  kMedium = 3,
  kLarge = 4,  // System default.
  kExtraLarge = 5,
  kExtraExtraLarge = 6,
  kExtraExtraExtraLarge = 7,
  kAccessibilityMedium = 8,
  kAccessibilityLarge = 9,
  kAccessibilityExtraLarge = 10,
  kAccessibilityExtraExtraLarge = 11,
  kAccessibilityExtraExtraExtraLarge = 12,
  kMaxValue = kAccessibilityExtraExtraExtraLarge,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:IOSContentSizeCategory)

// Returns the `IOSContentSizeCategory` value for
// `UIApplication.sharedApplication.preferredContentSizeCategory`.
ui_util::IOSContentSizeCategory GetPreferredContentSizeCategory();

// Records metrics related to the system fonts.
void RecordSystemFontSizeMetrics();

// Returns system suggested font size multiplier (e.g. 1.5 if the font size
// should be 50% bigger) for the actual system preferred content size category.
float SystemSuggestedFontSizeMultiplier();

// Returns system suggested font size multiplier (e.g. 1.5 if the font size
// should be 50% bigger) for the given `category`.
float SystemSuggestedFontSizeMultiplier(UIContentSizeCategory category);

// Returns system suggested font size multiplier (e.g. 1.5 if the font size
// should be 50% bigger) for the given `category`. The multiplier is clamped
// between the multipliers associated with `min_category` and `max_category`.
float SystemSuggestedFontSizeMultiplier(UIContentSizeCategory category,
                                        UIContentSizeCategory min_category,
                                        UIContentSizeCategory max_category);

}  // namespace ui_util

#endif  // IOS_COMPONENTS_UI_UTIL_DYNAMIC_TYPE_UTIL_H_
