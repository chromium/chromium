// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_FIELD_TRIAL_H_
#define IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_FIELD_TRIAL_H_

#import "base/metrics/field_trial.h"
#import "components/variations/variations_associated_data.h"

class PrefService;
class PrefRegistrySimple;

namespace new_tab_page_field_trial {

// Creates a field trial to control the improved popular sites default
// suggestions experiment so that it is shown on the NTP after first run.
//
// The trial group chosen on first run is persisted to local state prefs.
void Create(const base::FieldTrial::EntropyProvider& low_entropy_provider,
            base::FeatureList* feature_list,
            PrefService* local_state);

// Registers the local state pref used to manage grouping for this field trial.
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

// Exposes CreateNewTabPageFieldTrialForTesting() for testing FieldTrial
// set-up.
void CreateNewTabPageFieldTrialForTesting(
    std::map<variations::VariationID, int> weights_by_id,
    const base::FieldTrial::EntropyProvider& low_entropy_provider,
    base::FeatureList* feature_list);

}  // namespace new_tab_page_field_trial

#endif  // IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_FIELD_TRIAL_H_
