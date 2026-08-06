// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/model/features.h"

#import "base/metrics/field_trial_params.h"

BASE_FEATURE(kShareDefaultBrowserStatus, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsShareDefaultBrowserStatusEnabled() {
  return base::FeatureList::IsEnabled(kShareDefaultBrowserStatus);
}

BASE_FEATURE(kIOSSettingsDefaultBrowserPromoV2,
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kIOSSettingsDefaultBrowserPromoTypeParam[] =
    "SettingsDefaultBrowserPromoType";

BASE_FEATURE_PARAM(
    int,
    kIOSSettingsDefaultBrowserPromoTypeFeatureParam,
    &kIOSSettingsDefaultBrowserPromoV2,
    kIOSSettingsDefaultBrowserPromoTypeParam,
    static_cast<int>(
        SettingsDefaultBrowserPromoType::kSettingsDefaultBrowserCard));

bool IsIOSSettingsDefaultBrowserPromoV2Enabled() {
  return base::FeatureList::IsEnabled(kIOSSettingsDefaultBrowserPromoV2);
}

SettingsDefaultBrowserPromoType CurrentSettingsDefaultBrowserPromoType() {
  CHECK(IsIOSSettingsDefaultBrowserPromoV2Enabled());
  return static_cast<SettingsDefaultBrowserPromoType>(
      kIOSSettingsDefaultBrowserPromoTypeFeatureParam.Get());
}

BASE_FEATURE(kOmniboxPastePromoExperiment, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(int,
                   kOmniboxPastePromoExperimentType,
                   &kOmniboxPastePromoExperiment,
                   "arm",
                   1);

int GetOmniboxPastePromoExperimentType() {
  return kOmniboxPastePromoExperimentType.Get();
}
