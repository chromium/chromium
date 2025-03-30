// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_BEST_FEATURES_UI_METRICS_UTIL_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_BEST_FEATURES_UI_METRICS_UTIL_H_

enum class BestFeaturesItemType;

// Interactions with the Best Features detail screen. This is mapped to the
// IOSBestFeaturesDetailScreenActionType enum in enums.xml for metrics. Entries
// should not be renumbered and numeric values should never be reused.
// LINT.IfChange(BestFeaturesDetailScreenActionType)
enum class BestFeaturesDetailScreenActionType {
  kShowMeHow = 0,
  kContinueInFRESequence = 1,
  kNavigateBack = 2,
  kMaxValue = kNavigateBack,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSBestFeaturesDetailScreenActionType)

// Interactions with the Best Features main screen. This is mapped to the
// IOSBestFeaturesMainScreenActionType enum in enums.xml for metrics. Entries
// should not be renumbered and numeric values should never be reused.
// LINT.IfChange(BestFeaturesMainScreenActionType)
enum class BestFeaturesMainScreenActionType {
  kContinueWithoutInteracting = 0,
  kLensItemTapped = 1,
  kEnhancedSafeBrowsingItemTapped = 2,
  kLockedIncognitoTabsItemTapped = 3,
  kSaveAutofillPasswordsItemTapped = 4,
  kTabGroupsTapped = 5,
  kPriceTrackingTapped = 6,
  kPasswordsInOtherAppsItemTapped = 7,
  kSharePasswordsItemTapped = 8,
  kMaxValue = kSharePasswordsItemTapped,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSBestFeaturesMainScreenActionType)

// Name of histogram that logs interactions with the Best Features Main Screen.
extern const char kActionOnBestFeaturesMainScreenHistogram[];

// Name of histograms that logs interactions with the Best Features Detail
// Screens.
extern const char kActionOnBestFeaturesLensScreenHistogram[];
extern const char kActionOnBestFeaturesEnhancedSafeBrowsingHistogram[];
extern const char kActionOnBestFeaturesLockedIncognitoTabsHistogram[];
extern const char kActionOnBestFeaturesSaveAutofillPasswordsHistogram[];
extern const char kActionOnBestFeaturesTabGroupsHistogram[];
extern const char kActionOnBestFeaturesPriceTrackingHistogram[];
extern const char kActionOnBestFeaturesPasswordsInOtherAppsHistogram[];
extern const char kActionOnBestFeaturesSharePasswordsHistogram[];

// Returns the name of the histogram for logging action on a Best Features
// screen of type `item_type`.
const char* BestFeaturesActionHistogramForItemType(
    BestFeaturesItemType item_type);

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_BEST_FEATURES_UI_METRICS_UTIL_H_
