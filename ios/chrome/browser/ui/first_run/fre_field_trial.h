// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_FRE_FIELD_TRIAL_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_FRE_FIELD_TRIAL_H_

#include "base/metrics/field_trial.h"
#include "components/variations/variations_associated_data.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class FeatureList;
}  // namespace base

// Version of the new Default Browser Promo FRE to show.
enum class NewDefaultBrowserPromoFRE {
  // FRE default browser promo only.
  kFirstRunOnly = 0,
  // Wait 3 days after FRE default browser promo.
  kShortDelay,
  // Wait 14 days after FRE default browser promo.
  kDefaultDelay,
  // FRE default browser promo not enabled.
  kDisabled,
};

// Version of the new MICE FRE to show.
enum class NewMobileIdentityConsistencyFRE {
  // New MICE FRE with tangible sync (welcome with sign-in + tangible sync
  // screens).
  // Strings in TangibleSyncViewController are set according to the A, B or C
  // variants.
  kTangibleSyncA = 0,
  kTangibleSyncB,
  kTangibleSyncC,
  // New MICE FRE with 2 steps (welcome with sign-in + sync screens).
  kTwoSteps,
  // Old FRE.
  kOld,
  // New MICE FRE with tangible sync (welcome with sign-in + tangible sync
  // screens).
  // Strings in TangibleSyncViewController are set according to the D, E or F
  // variants.
  kTangibleSyncD,
  kTangibleSyncE,
  kTangibleSyncF,
};

namespace fre_field_trial {

// Returns the FRE default browser promo setup according to the feature flag and
// experiment. See NewDefaultBrowserPromoFRE.
NewDefaultBrowserPromoFRE GetFREDefaultBrowserScreenPromoFRE();

// Returns the FRE to display according to the feature flag and experiment.
// See NewMobileIdentityConsistencyFRE.
NewMobileIdentityConsistencyFRE GetNewMobileIdentityConsistencyFRE();

// Registers the local state pref used to manage grouping for this field trial.
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

// Creates a field trial to control features that need to be used on first run,
// including the LocationPermissions feature and FRE experiments.
//
// The trial group chosen on first run is persisted to local state prefs.
void Create(const base::FieldTrial::EntropyProvider& low_entropy_provider,
            base::FeatureList* feature_list,
            PrefService* local_state);

namespace testing {

// Exposes CreateNewMICeAndDefaultBrowserFRETrial() for testing FieldTrial
// set-up.
int CreateNewMICeAndDefaultBrowserFRETrialForTesting(
    const std::map<variations::VariationID, int>& weight_by_id,
    const base::FieldTrial::EntropyProvider& low_entropy_provider,
    base::FeatureList* feature_list);

}  // namespace testing

}  // namespace fre_field_trial

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_FRE_FIELD_TRIAL_H_
