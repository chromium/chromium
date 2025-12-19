// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_promo/ui_bundled/public/features.h"

#import "base/metrics/field_trial_params.h"

const char kDefaultBrowserPromoRefreshParam[] =
    "DefaultBrowserPromoRefreshParam";
const char kDefaultBrowserPromoRefreshParamNoInstructions[] = "NoInstructions";
const char kDefaultBrowserPromoRefreshParamSystemAlertInstructions[] =
    "SystemAlertInstructions";
const char kDefaultBrowserPromoRefreshParamPictureInPictureInstructions[] =
    "PictureInPictureInstructions";
const char kDefaultBrowserPromoRefreshParamCarouselInstructions[] =
    "CarouselInstructions";

BASE_FEATURE(kDefaultBrowserPromoIpadInstructions,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDefaultBrowserPromoRefresh, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsDefaultBrowserPromoIpadInstructions() {
  return base::FeatureList::IsEnabled(kDefaultBrowserPromoIpadInstructions);
}

bool IsDefaultBrowserPromoRefreshEnabled() {
  return base::FeatureList::IsEnabled(kDefaultBrowserPromoRefresh);
}

std::string DefaultBrowserPromoRefreshParam() {
  return base::GetFieldTrialParamByFeatureAsString(
      kDefaultBrowserPromoRefresh, kDefaultBrowserPromoRefreshParam,
      kDefaultBrowserPromoRefreshParamNoInstructions);
}
