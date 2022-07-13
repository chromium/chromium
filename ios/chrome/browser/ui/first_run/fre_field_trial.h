// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_FRE_FIELD_TRIAL_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_FRE_FIELD_TRIAL_H_

#include "base/metrics/field_trial.h"
#include "components/variations/variations_associated_data.h"

class PrefRegistrySimple;
class PrefService;

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
  // Old FRE with UMA dialog.
  kUMADialog = 0,
  // New MICE FRE with 3 steps (welcome + sign-in + sync screens).
  kThreeSteps,
  // New MICE FRE with 2 steps (welcome with sign-in + sync screens).
  kTwoSteps,
  // Old FRE.
  kOld,
};

namespace base {
class FeatureList;
}  // namespace base

// Name of current experiment.
extern const char kIOSMICeAndDefaultBrowserTrialName[];

// Indicates which FRE default browser promo variant to use.
extern const char kFREDefaultBrowserPromoParam[];

// Indicates if the FRE default browser promo variant "Wait 14 days after FRE
// default browser promo" is enabled.
extern const char kFREDefaultBrowserPromoDefaultDelayParam[];

// Indicates if the FRE default browser promo variant "FRE default browser
// promo only" is enabled.
extern const char kFREDefaultBrowserPromoFirstRunOnlyParam[];

// Indicates if the FRE default browser promo variant "Wait 3 days after FRE
// default promo" is enabled.
extern const char kFREDefaultBrowserPromoShortDelayParam[];

// Indicates which variant of the new MICE FRE to use.
extern const char kNewMobileIdentityConsistencyFREParam[];
extern const char kNewMobileIdentityConsistencyFREParamUMADialog[];
extern const char kNewMobileIdentityConsistencyFREParamThreeSteps[];
extern const char kNewMobileIdentityConsistencyFREParamTwoSteps[];

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
