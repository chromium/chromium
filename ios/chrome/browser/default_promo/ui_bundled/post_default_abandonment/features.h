// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_POST_DEFAULT_ABANDONMENT_FEATURES_H_
#define IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_POST_DEFAULT_ABANDONMENT_FEATURES_H_

#import "base/feature_list.h"

#import "base/metrics/field_trial_params.h"

// Feature flag that enables the post-default browser abandonment promo.
BASE_DECLARE_FEATURE(kPostDefaultAbandonmentPromo);

// The following params define the interval to use when determining eligibility
// for the post-default browser abandonment promo. The integers represent a
// number of days in the past. For example, if the start param is 50 and the end
// param is 25, then the interval being considered is "between 50 and 25 days
// ago". Note that these values are converted to base::TimeDelta when doing the
// interval check, so a value of 1 means "in the last 24 hours" and not "at any
// point yesterday".
extern const base::FeatureParam<int> kPostDefaultAbandonmentIntervalStart;
extern const base::FeatureParam<int> kPostDefaultAbandonmentIntervalEnd;

// Returns true if the post-default browser abandonment promo feature is
// enabled.
bool IsPostDefaultAbandonmentPromoEnabled();

// Returns true if the user's last external URL open is within the interval
// defined by the start and end parameters of the post-default browser
// abandonment feature.
bool IsEligibleForPostDefaultAbandonmentPromo();

#endif  // IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_POST_DEFAULT_ABANDONMENT_FEATURES_H_
