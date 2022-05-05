// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_FRE_FIELD_TRIAL_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_FRE_FIELD_TRIAL_H_

#include "base/metrics/field_trial.h"

class PrefRegistrySimple;
class PrefService;

enum class SigninSyncScreenUIIdentitySwitcherPosition : int {
  kTop,
  kBottom,
};

enum class SigninSyncScreenUIStringSet : int {
  kOld,
  kNew,
};

// Version of the new MICE FRE to show.
enum class NewMobileIdentityConsistencyFRE : int {
  // Old FRE with UMA dialog.
  kUMADialog,
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

// Indicates if the FRE default browser promo variant "Wait 14 days after FRE
// default browser promo" is enabled.
extern const char kFREDefaultPromoTestingDefaultDelayParam[];

// Indicates if the FRE default browser promo variant "FRE default browser
// promo only" is enabled.
extern const char kFREDefaultPromoTestingOnlyParam[];

// Indicates if the FRE default browser promo variant "Wait 3 days after FRE
// default promo" is enabled.
extern const char kFREDefaultPromoTestingShortDelayParam[];

// Indicates which variant of the new MICE FRE to use.
extern const char kNewMobileIdentityConsistencyFREParam[];
extern const char kNewMobileIdentityConsistencyFREParamUMADialog[];
extern const char kNewMobileIdentityConsistencyFREParamThreeSteps[];
extern const char kNewMobileIdentityConsistencyFREParamTwoSteps[];

// Group names for the new Mobile Identity Consistency FRE.
extern const char kNewMICEFREWithUMADialogSetGroup[];
extern const char kNewMICEFREWithThreeStepsSetGroup[];
extern const char kNewMICEFREWithTwoStepsSetGroup[];

namespace fre_field_trial {

// Returns true if the user is in the group that will show the default browser
// screen in first run (FRE) with activate a short cooldown of other default
// browser promos.
bool IsInFirstRunDefaultBrowserAndSmallDelayBeforeOtherPromosGroup();

// Returns true if the user is in the group that will show the default browser
// screen in first run (FRE) and activate cooldown of other default browser
// promos.
bool IsInFirstRunDefaultBrowserAndDefaultDelayBeforeOtherPromosGroup();

// Returns true if the user is in the group that will show the default browser
// screen in first run (FRE) only.
bool IsInDefaultBrowserPromoAtFirstRunOnlyGroup();

// Returns true if the default browser screen in FRE is enabled.
bool IsFREDefaultBrowserScreenEnabled();

// TODO(crbug.com/1322292): Remove.
// Returns the UI option for the sign-in & sync screen identity position.
SigninSyncScreenUIIdentitySwitcherPosition
GetSigninSyncScreenUIIdentitySwitcherPosition();

// TODO(crbug.com/1322292): Remove.
// Returns the UI option for the sign-in & sync screen strings set.
SigninSyncScreenUIStringSet GetSigninSyncScreenUIStringSet();

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

}  // namespace fre_field_trial

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_FRE_FIELD_TRIAL_H_
