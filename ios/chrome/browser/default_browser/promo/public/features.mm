// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/promo/public/features.h"

#import "base/metrics/field_trial_params.h"
#import "ios/chrome/browser/shared/public/features/features.h"

const char kDefaultBrowserPictureInPictureParam[] =
    "DefaultBrowserPictureInPictureParam";
const char kDefaultBrowserPictureInPictureParamEnabled[] = "Enabled";
const char kDefaultBrowserPictureInPictureParamDisabledDefaultApps[] =
    "DisabledDefaultApps";
const char kDefaultBrowserPictureInPictureParamEnabledDefaultApps[] =
    "EnabledDefaultApps";

BASE_FEATURE(kDefaultBrowserPromoIpadInstructions,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDefaultBrowserPictureInPicture,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsDefaultBrowserPromoIpadInstructions() {
  return base::FeatureList::IsEnabled(kDefaultBrowserPromoIpadInstructions);
}

bool IsDefaultBrowserPictureInPictureEnabled() {
  return IsDefaultAppsDestinationAvailable() &&
         base::FeatureList::IsEnabled(kDefaultBrowserPictureInPicture);
}

std::string DefaultBrowserPictureInPictureParam() {
  return base::GetFieldTrialParamByFeatureAsString(
      kDefaultBrowserPictureInPicture, kDefaultBrowserPictureInPictureParam,
      kDefaultBrowserPictureInPictureParamEnabled);
}

bool IsDefaultAppsPictureInPictureVariant() {
  if (!IsDefaultAppsDestinationAvailable()) {
    return false;
  }

  const std::string pipParam = DefaultBrowserPictureInPictureParam();
  return pipParam == kDefaultBrowserPictureInPictureParamEnabledDefaultApps ||
         pipParam == kDefaultBrowserPictureInPictureParamDisabledDefaultApps;
}
