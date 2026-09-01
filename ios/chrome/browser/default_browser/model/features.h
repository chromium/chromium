// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_FEATURES_H_
#define IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_FEATURES_H_

#import "base/feature_list.h"
#import "base/metrics/field_trial_params.h"

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

// Feature to enable the Default Browser Promo in the Overflow Menu.
BASE_DECLARE_FEATURE(kDefaultBrowserPromoOverflowMenu);

// Parameter name for the Default Browser Promo Overflow Menu type.
extern const char kDefaultBrowserPromoOverflowMenuTypeParam[];

// Enum defining the available overflow menu default browser promo types.
enum class DefaultBrowserPromoOverflowMenuType {
  kDestination = 0,
  kShortcuts = 1,
};

// Returns whether `kDefaultBrowserPromoOverflowMenu` is enabled.
bool IsDefaultBrowserPromoOverflowMenuEnabled();

// Returns the current `DefaultBrowserPromoOverflowMenuType` based on feature
// parameters.
DefaultBrowserPromoOverflowMenuType
CurrentDefaultBrowserPromoOverflowMenuType();

// Feature to enable the Omnibox Paste Flow copy experiments.
BASE_DECLARE_FEATURE(kOmniboxPastePromoExperiment);

// Returns the active experiment arm (1 through 10).
int GetOmniboxPastePromoExperimentType();

// Feature to display a text video in the default browser picture in picture.
BASE_DECLARE_FEATURE(kDefaultBrowserPipTextVideo);

// Returns whether `kDefaultBrowserPipTextVideo` is enabled.
bool IsDefaultBrowserPipTextVideoEnabled();

#endif  // IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_FEATURES_H_
