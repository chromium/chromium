// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_TRENDING_QUERIES_FIELD_TRIAL_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_TRENDING_QUERIES_FIELD_TRIAL_H_

#include "base/metrics/field_trial.h"
#include "components/variations/variations_associated_data.h"

class PrefService;

extern const char kTrendingQueriesFieldTrialName[];

namespace {

// Variation IDs for Trending Queries experiment arms.
const variations::VariationID kTrendingQueriesEnabledAllUsersID = 3350760;
const variations::VariationID kTrendingQueriesEnabledAllUsersHideShortcutsID =
    3350761;
const variations::VariationID kTrendingQueriesEnabledDisabledFeedID = 3350762;
const variations::VariationID kTrendingQueriesEnabledSignedOutID = 3350763;
const variations::VariationID kTrendingQueriesEnabledNeverShowModuleID =
    4833277;
const variations::VariationID kTrendingQueriesControlID = 3350764;

}  // namespace

namespace trending_queries_field_trial {

// Creates a field trial to control the Trending Queries feature so that it is
// shown on the NTP after first run.
//
// The trial group chosen on first run is persisted to local state prefs.
void Create(const base::FieldTrial::EntropyProvider& low_entropy_provider,
            base::FeatureList* feature_list,
            PrefService* local_state);

// Exposes CreateTrendingQueriesTrial() for testing FieldTrial set-up.
void CreateTrendingQueriesTrialForTesting(
    std::map<variations::VariationID, int> weights_by_id,
    const base::FieldTrial::EntropyProvider& low_entropy_provider,
    base::FeatureList* feature_list);

}  // namespace trending_queries_field_trial

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_TRENDING_QUERIES_FIELD_TRIAL_H_
