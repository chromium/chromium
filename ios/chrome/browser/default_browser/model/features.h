// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_FEATURES_H_
#define IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_FEATURES_H_

#import "base/feature_list.h"

// Feature to enable sharing default browser status with 1P apps.
BASE_DECLARE_FEATURE(kShareDefaultBrowserStatus);

// Returns whether `kShareDefaultBrowserStatus` is enabled.
bool IsShareDefaultBrowserStatusEnabled();

// Feature to enable the Settings Default Browser Promo V2.
BASE_DECLARE_FEATURE(kIOSSettingsDefaultBrowserPromoV2);

// Parameter name for the Settings Default Browser Promo type.
extern const char kIOSSettingsDefaultBrowserPromoTypeParam[];

// Enum defining the available settings default browser promo types.
enum class SettingsDefaultBrowserPromoType {
  kSettingsDefaultBrowserCard = 0,
  kSettingsDefaultBrowserCell = 1,
};

// Returns whether `kIOSSettingsDefaultBrowserPromoV2` is enabled.
bool IsIOSSettingsDefaultBrowserPromoV2Enabled();

// Returns the current `SettingsDefaultBrowserPromoType` based on feature
// parameters.
SettingsDefaultBrowserPromoType CurrentSettingsDefaultBrowserPromoType();

#endif  // IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_FEATURES_H_
