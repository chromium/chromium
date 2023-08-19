// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/overflow_menu/feature_flags.h"

#import "components/password_manager/core/common/password_manager_features.h"

BASE_FEATURE(kNewOverflowMenu,
             "NewOverflowMenu",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSmartSortingPriceTrackingDestination,
             "kSmartSortingPriceTrackingDestination",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNewOverflowMenuShareChromeAction,
             "kNewOverflowMenuShareChromeAction",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kOverflowMenuCustomization,
             "OverflowMenuCustomization",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsNewOverflowMenuEnabled() {
  if (@available(iOS 15, *)) {
    return base::FeatureList::IsEnabled(kNewOverflowMenu);
  }
  // The new overflow menu isn't available on iOS <= 14 because it relies on
  // `UISheetPresentationController`, which was introduced in iOS 15.
  return false;
}

bool IsSmartSortingPriceTrackingDestinationEnabled() {
  return base::FeatureList::IsEnabled(kSmartSortingPriceTrackingDestination);
}

bool IsNewOverflowMenuShareChromeActionEnabled() {
  return IsNewOverflowMenuEnabled() &&
         base::FeatureList::IsEnabled(kNewOverflowMenuShareChromeAction);
}

bool IsOverflowMenuCustomizationEnabled() {
  return IsNewOverflowMenuEnabled() &&
         base::FeatureList::IsEnabled(kOverflowMenuCustomization);
}
