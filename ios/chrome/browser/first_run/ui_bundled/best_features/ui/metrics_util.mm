// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/best_features/ui/metrics_util.h"

#import "base/metrics/histogram_functions.h"
#import "ios/chrome/browser/first_run/public/best_features_item.h"

const char kActionOnBestFeaturesMainScreenHistogram[] =
    "IOS.FirstRun.BestFeatures.MainScreen.Action";
const char kActionOnBestFeaturesLensScreenHistogram[] =
    "IOS.FirstRun.BestFeatures.Lens.Action";
const char kActionOnBestFeaturesEnhancedSafeBrowsingHistogram[] =
    "IOS.FirstRun.BestFeatures.EnhancedSafeBrowsing.Action";
const char kActionOnBestFeaturesLockedIncognitoTabsHistogram[] =
    "IOS.FirstRun.BestFeatures.LockedIncognitoTabs.Action";
const char kActionOnBestFeaturesSaveAutofillPasswordsHistogram[] =
    "IOS.FirstRun.BestFeatures.SaveAutofillPasswords.Action";
const char kActionOnBestFeaturesTabGroupsHistogram[] =
    "IOS.FirstRun.BestFeatures.TabGroups.Action";
const char kActionOnBestFeaturesPriceTrackingHistogram[] =
    "IOS.FirstRun.BestFeatures.PriceTracking.Action";
const char kActionOnBestFeaturesPasswordsInOtherAppsHistogram[] =
    "IOS.FirstRun.BestFeatures.PasswordsInOtherApps.Action";
const char kActionOnBestFeaturesSharePasswordsHistogram[] =
    "IOS.FirstRun.BestFeatures.SharePasswords.Action";

const char* BestFeaturesActionHistogramForItemType(
    BestFeaturesItemType item_type) {
  using enum BestFeaturesItemType;
  switch (item_type) {
    case kLensSearch:
      return kActionOnBestFeaturesLensScreenHistogram;
    case kEnhancedSafeBrowsing:
      return kActionOnBestFeaturesEnhancedSafeBrowsingHistogram;
    case kLockedIncognitoTabs:
      return kActionOnBestFeaturesLockedIncognitoTabsHistogram;
    case kSaveAndAutofillPasswords:
      return kActionOnBestFeaturesSaveAutofillPasswordsHistogram;
    case kTabGroups:
      return kActionOnBestFeaturesTabGroupsHistogram;
    case kPriceTrackingAndInsights:
      return kActionOnBestFeaturesPriceTrackingHistogram;
    case kAutofillPasswordsInOtherApps:
      return kActionOnBestFeaturesPasswordsInOtherAppsHistogram;
    case kSharePasswordsWithFamily:
      return kActionOnBestFeaturesSharePasswordsHistogram;
  }
}
