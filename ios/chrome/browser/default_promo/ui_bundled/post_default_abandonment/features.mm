// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_promo/ui_bundled/post_default_abandonment/features.h"

#import "ios/chrome/browser/default_browser/model/utils.h"

BASE_FEATURE(kPostDefaultAbandonmentPromo,
             "PostDefaultAbandonmentPromo",
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<int> kPostDefaultAbandonmentIntervalStart{
    &kPostDefaultAbandonmentPromo,
    /*name=*/"postDefaultAbandonmentIntervalStart", /*default_value=*/21};

constexpr base::FeatureParam<int> kPostDefaultAbandonmentIntervalEnd{
    &kPostDefaultAbandonmentPromo, /*name=*/"postDefaultAbandonmentIntervalEnd",
    /*default_value=*/7};

bool IsPostDefaultAbandonmentPromoEnabled() {
  return base::FeatureList::IsEnabled(kPostDefaultAbandonmentPromo);
}

bool IsEligibleForPostDefaultAbandonmentPromo() {
  return IsPostDefaultAbandonmentPromoEnabled() &&
         IsChromePotentiallyNoLongerDefaultBrowser(
             kPostDefaultAbandonmentIntervalStart.Get(),
             kPostDefaultAbandonmentIntervalEnd.Get());
}
