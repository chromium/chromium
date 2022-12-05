// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/overflow_menu/feature_flags.h"

#import "components/password_manager/core/common/password_manager_features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BASE_FEATURE(kNewOverflowMenu,
             "NewOverflowMenu",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSmartSortingNewOverflowMenu,
             "kSmartSortingNewOverflowMenu",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kNewOverflowMenuShareChromeAction,
             "kNewOverflowMenuShareChromeAction",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNewOverflowMenuAlternateIPH,
             "NewOverflowMenuAlternateIPH",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsNewOverflowMenuEnabled() {
  if (@available(iOS 15, *)) {
    return base::FeatureList::IsEnabled(kNewOverflowMenu);
  }
  // The new overflow menu isn't available on iOS <= 14 because it relies on
  // `UISheetPresentationController`, which was introduced in iOS 15.
  return false;
}

bool IsPasswordManagerBrandingUpdateEnabled() {
  if (IsNewOverflowMenuEnabled()) {
    return base::FeatureList::IsEnabled(
        password_manager::features::kIOSEnablePasswordManagerBrandingUpdate);
  }

  return false;
}

bool IsSmartSortingNewOverflowMenuEnabled() {
  return IsNewOverflowMenuEnabled() &&
         base::FeatureList::IsEnabled(kSmartSortingNewOverflowMenu);
}

bool IsNewOverflowMenuShareChromeActionEnabled() {
  return IsNewOverflowMenuEnabled() &&
         base::FeatureList::IsEnabled(kNewOverflowMenuShareChromeAction);
}

bool IsNewOverflowMenuAlternateIPHEnabled() {
  return IsNewOverflowMenuEnabled() &&
         base::FeatureList::IsEnabled(kNewOverflowMenuAlternateIPH);
}
