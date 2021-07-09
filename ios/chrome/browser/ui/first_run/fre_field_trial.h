// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_FRE_FIELD_TRIAL_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_FRE_FIELD_TRIAL_H_

#include "base/metrics/field_trial.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class FeatureList;
}  // namespace base

namespace fre_field_trial {

// Returns true if the user is in the group that will be shown the First Run
// Modal.
bool IsInFirstRunModalGroup();

// Returns true if the user is in the group that will have the location prompt
// removed from First Run.
bool IsInRemoveFirstRunPromptGroup();

// Registers the local state pref used to manage grouping for this field trial.
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

// Creates a field trial to control the LocationPermissions feature. The trial
// is client controlled because one arm of the experiment involves changing the
// user experience during First Run.
//
// The trial group chosen on first run is persisted to local state prefs.
void Create(const base::FieldTrial::EntropyProvider& low_entropy_provider,
            base::FeatureList* feature_list,
            PrefService* local_state);

}  // namespace fre_field_trial

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_FRE_FIELD_TRIAL_H_
