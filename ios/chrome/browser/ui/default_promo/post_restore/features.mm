// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/default_promo/post_restore/features.h"

#import "base/metrics/field_trial_params.h"

BASE_FEATURE(kPostRestoreDefaultBrowserPromo,
             "PostRestoreDefaultBrowserPromo",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kPostRestoreDefaultBrowserPromoHalfscreenParam[] =
    "post-restore-default-browser-promo-halfscreen";
const char kPostRestoreDefaultBrowserPromoFullscreenParam[] =
    "post-restore-default-browser-promo-fullscreen";

PostRestoreDefaultBrowserPromoType GetPostRestoreDefaultBrowserPromoType() {
  if (!base::FeatureList::IsEnabled(kPostRestoreDefaultBrowserPromo)) {
    return PostRestoreDefaultBrowserPromoType::kDisabled;
  }
  if (base::GetFieldTrialParamByFeatureAsBool(
          kPostRestoreDefaultBrowserPromo,
          kPostRestoreDefaultBrowserPromoHalfscreenParam, false)) {
    return PostRestoreDefaultBrowserPromoType::kHalfscreen;
  }
  if (base::GetFieldTrialParamByFeatureAsBool(
          kPostRestoreDefaultBrowserPromo,
          kPostRestoreDefaultBrowserPromoFullscreenParam, false)) {
    return PostRestoreDefaultBrowserPromoType::kFullscreen;
  }
  return PostRestoreDefaultBrowserPromoType::kAlert;
}
