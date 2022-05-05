// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/first_run/fre_field_trial.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/signin/ios/browser/features.h"
#include "components/variations/variations_associated_data.h"
#include "components/version_info/version_info.h"
#include "ios/chrome/browser/first_run/first_run.h"
#include "ios/chrome/browser/ui/first_run/ios_first_run_field_trials.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/common/channel_info.h"

const char kFREDefaultPromoTestingDefaultDelayParam[] =
    "variant_default_delay_enabled";
const char kFREDefaultPromoTestingOnlyParam[] = "variant_fre_only_enabled";
const char kFREDefaultPromoTestingShortDelayParam[] =
    "variant_short_delay_enabled";

// Parameters for new Mobile Identity Consistency FRE.
const char kNewMobileIdentityConsistencyFREParam[] = "variant_new_mice_fre";
const char kNewMobileIdentityConsistencyFREParamUMADialog[] = "umadialog";
const char kNewMobileIdentityConsistencyFREParamThreeSteps[] = "3steps";
const char kNewMobileIdentityConsistencyFREParamTwoSteps[] = "2steps";

// Group names for the new Mobile Identity Consistency FRE.
const char kNewMICEFREWithUMADialogSetGroup[] =
    "NewMobileIdentityConsistencyFREParamUMADialog";
const char kNewMICEFREWithThreeStepsSetGroup[] =
    "NewMobileIdentityConsistencyFREParamThreeSteps";
const char kNewMICEFREWithTwoStepsSetGroup[] =
    "NewMobileIdentityConsistencyFREParamTwoSteps";

namespace {

// Group names for the default browser promo trial.
const char kFREDefaultBrowserAndSmallDelayBeforeOtherPromosGroup[] =
    "FREDefaultBrowserAndSmallDelayBeforeOtherPromos";
const char kFREDefaultBrowserAndDefaultDelayBeforeOtherPromosGroup[] =
    "FREDefaultBrowserAndDefaultDelayBeforeOtherPromos";
const char kDefaultBrowserPromoAtFirstRunOnlyGroup[] =
    "DefaultBrowserPromoAtFirstRunOnly";
// Group names for the FRE redesign permissions trial.
const char kDefaultGroup[] = "Default";
// Experiment IDs defined for the above field trial groups.
const variations::VariationID kDefaultTrialID = 3330131;
const variations::VariationID kDefaultBrowserPromoAtFirstRunOnlyID = 3342136;
const variations::VariationID
    kFREDefaultBrowserAndDefaultDelayBeforeOtherPromosID = 3342137;
const variations::VariationID
    kFREDefaultBrowserAndSmallDelayBeforeOtherPromosID = 3342138;
// Group name for the FRE disabled group.
const char kDisabledGroup[] = "Disabled";
const variations::VariationID kDisabledTrialID = 3346917;

// Options for kkNewMobileIdentityConsistencyFREParam.
constexpr base::FeatureParam<NewMobileIdentityConsistencyFRE>::Option
    kNewMobileIdentityConsistencyFREOptions[] = {
        {NewMobileIdentityConsistencyFRE::kUMADialog,
         kNewMobileIdentityConsistencyFREParamUMADialog},
        {NewMobileIdentityConsistencyFRE::kThreeSteps,
         kNewMobileIdentityConsistencyFREParamThreeSteps},
        {NewMobileIdentityConsistencyFRE::kTwoSteps,
         kNewMobileIdentityConsistencyFREParamTwoSteps}};

// Parameter for signin::kNewMobileIdentityConsistencyFRE feature.
constexpr base::FeatureParam<NewMobileIdentityConsistencyFRE>
    kkNewMobileIdentityConsistencyFREParam{
        &signin::kNewMobileIdentityConsistencyFRE,
        kNewMobileIdentityConsistencyFREParam,
        NewMobileIdentityConsistencyFRE::kUMADialog,
        &kNewMobileIdentityConsistencyFREOptions};

// Experiment IDs defined for the second trial of the FRE UI.
const variations::VariationID kNewMICEFREWithUMADialogSetID = 3346235;
const variations::VariationID kNewMICEFREWithThreeStepsSetID = 3346236;
const variations::VariationID kNewMICEFREWithTwoStepsSetID = 3346237;

// Sets the parameter value of the new MICE FRE parameter.
void AssociateFieldTrialParamsForNewMICEFREGroup(const std::string& group_name,
                                                 const std::string& value) {
  base::FieldTrialParams params;
  params[kNewMobileIdentityConsistencyFREParam] = value;
  bool association_result = base::AssociateFieldTrialParams(
      signin::kNewMobileIdentityConsistencyFRE.name, group_name, params);
  DCHECK(association_result);
}

}  // namespace

namespace fre_field_trial {

bool IsInFirstRunDefaultBrowserAndSmallDelayBeforeOtherPromosGroup() {
  if (base::FeatureList::IsEnabled(kEnableFREDefaultBrowserScreenTesting)) {
    return base::GetFieldTrialParamByFeatureAsBool(
        kEnableFREDefaultBrowserScreenTesting,
        kFREDefaultPromoTestingShortDelayParam, false);
  }
  return base::FeatureList::IsEnabled(kEnableFREUIModuleIOS) &&
         base::FieldTrialList::FindFullName(kEnableFREUIModuleIOS.name) ==
             kFREDefaultBrowserAndSmallDelayBeforeOtherPromosGroup;
}

bool IsInFirstRunDefaultBrowserAndDefaultDelayBeforeOtherPromosGroup() {
  if (base::FeatureList::IsEnabled(kEnableFREDefaultBrowserScreenTesting)) {
    return base::GetFieldTrialParamByFeatureAsBool(
        kEnableFREDefaultBrowserScreenTesting,
        kFREDefaultPromoTestingDefaultDelayParam, false);
  }
  return base::FeatureList::IsEnabled(kEnableFREUIModuleIOS) &&
         base::FieldTrialList::FindFullName(kEnableFREUIModuleIOS.name) ==
             kFREDefaultBrowserAndDefaultDelayBeforeOtherPromosGroup;
}

bool IsInDefaultBrowserPromoAtFirstRunOnlyGroup() {
  if (base::FeatureList::IsEnabled(kEnableFREDefaultBrowserScreenTesting)) {
    return base::GetFieldTrialParamByFeatureAsBool(
        kEnableFREDefaultBrowserScreenTesting, kFREDefaultPromoTestingOnlyParam,
        false);
  }
  return base::FeatureList::IsEnabled(kEnableFREUIModuleIOS) &&
         base::FieldTrialList::FindFullName(kEnableFREUIModuleIOS.name) ==
             kDefaultBrowserPromoAtFirstRunOnlyGroup;
}

bool IsFREDefaultBrowserScreenEnabled() {
  return IsInFirstRunDefaultBrowserAndSmallDelayBeforeOtherPromosGroup() ||
         IsInFirstRunDefaultBrowserAndDefaultDelayBeforeOtherPromosGroup() ||
         IsInDefaultBrowserPromoAtFirstRunOnlyGroup();
}

SigninSyncScreenUIIdentitySwitcherPosition
GetSigninSyncScreenUIIdentitySwitcherPosition() {
  return SigninSyncScreenUIIdentitySwitcherPosition::kTop;
}

SigninSyncScreenUIStringSet GetSigninSyncScreenUIStringSet() {
  return SigninSyncScreenUIStringSet::kOld;
}

NewMobileIdentityConsistencyFRE GetNewMobileIdentityConsistencyFRE() {
  if (base::FeatureList::IsEnabled(signin::kNewMobileIdentityConsistencyFRE)) {
    return kkNewMobileIdentityConsistencyFREParam.Get();
  }
  return NewMobileIdentityConsistencyFRE::kOld;
}

// Creates a trial for the first run (when there is no variations seed) if
// necessary and enables the feature based on the randomly selected trial group.
// Returns the group number.
int CreateFirstRunTrial(
    base::FieldTrial::EntropyProvider const& low_entropy_provider,
    base::FeatureList* feature_list) {
  // New FRE enabled/disabled.
  int new_fre_default_percent = 0;

  // FRE's default browser screen experiment
  int new_fre_with_default_screen_and_short_cooldown_percent = 0;
  int new_fre_with_default_screen_and_default_cooldown_percent = 0;
  int new_fre_with_default_screen_only_percent = 0;

  switch (GetChannel()) {
    case version_info::Channel::UNKNOWN:
    case version_info::Channel::CANARY:
    case version_info::Channel::DEV:
    case version_info::Channel::BETA:
      new_fre_with_default_screen_and_short_cooldown_percent = 0;
      new_fre_with_default_screen_and_default_cooldown_percent = 0;
      new_fre_with_default_screen_only_percent = 0;
      new_fre_default_percent = 100;
      break;
    case version_info::Channel::STABLE:
      new_fre_with_default_screen_and_short_cooldown_percent = 0;
      new_fre_with_default_screen_and_default_cooldown_percent = 0;
      new_fre_with_default_screen_only_percent = 0;
      new_fre_default_percent = 100;
      break;
  }

  // Set up the trial and groups.
  FirstRunFieldTrialConfig config(kEnableFREUIModuleIOS.name);
  // Default browser promo experiment groups
  config.AddGroup(kFREDefaultBrowserAndSmallDelayBeforeOtherPromosGroup,
                  kFREDefaultBrowserAndSmallDelayBeforeOtherPromosID,
                  new_fre_with_default_screen_and_short_cooldown_percent);
  config.AddGroup(kFREDefaultBrowserAndDefaultDelayBeforeOtherPromosGroup,
                  kFREDefaultBrowserAndDefaultDelayBeforeOtherPromosID,
                  new_fre_with_default_screen_and_default_cooldown_percent);
  config.AddGroup(kDefaultBrowserPromoAtFirstRunOnlyGroup,
                  kDefaultBrowserPromoAtFirstRunOnlyID,
                  new_fre_with_default_screen_only_percent);

  // New FRE groups
  config.AddGroup(kDefaultGroup, kDefaultTrialID, new_fre_default_percent);

  DCHECK_EQ(100, config.GetTotalProbability());

  scoped_refptr<base::FieldTrial> trial =
      config.CreateOneTimeRandomizedTrial(kDefaultGroup, low_entropy_provider);

  // Finalize the group choice and activates the trial - similar to a variation
  // config that's marked with |starts_active| true. This is required for
  // studies that register variation ids, so they don't reveal extra information
  // beyond the low-entropy source.
  int group = trial->group();
  const std::string& group_name = trial->group_name();
  if (group_name == kFREDefaultBrowserAndSmallDelayBeforeOtherPromosGroup ||
      group_name == kFREDefaultBrowserAndDefaultDelayBeforeOtherPromosGroup ||
      group_name == kDefaultBrowserPromoAtFirstRunOnlyGroup) {
    feature_list->RegisterFieldTrialOverride(
        kEnableFREUIModuleIOS.name, base::FeatureList::OVERRIDE_ENABLE_FEATURE,
        trial.get());
  }
  return group;
}

// Creates the trial config, initialize the trial and returns the ID of the
// new Mobile Identity Consistency FRE trial group. There are 5 groups:
// - Control (Default)
// - Disabled
// - New MICE FRE with UMA dialog
// - New MICE FRE with 3 steps
// - New MICE FRE with 2 steps
int CreateNewMobileIdentityConsistencyFRETrial(
    const base::FieldTrial::EntropyProvider& low_entropy_provider,
    base::FeatureList* feature_list) {
  // Experiment groups
  int new_fre_default_percent = 0;
  int new_fre_disabled_percent = 0;
  int new_fre_with_uma_dialog_set_percent = 0;
  int new_fre_with_three_steps_set_percent = 0;
  int new_fre_with_two_steps_set_percent = 0;

  switch (GetChannel()) {
    case version_info::Channel::UNKNOWN:
    case version_info::Channel::CANARY:
    case version_info::Channel::DEV:
    case version_info::Channel::BETA:
    case version_info::Channel::STABLE:
      new_fre_disabled_percent = 0;
      new_fre_with_uma_dialog_set_percent = 0;
      new_fre_with_three_steps_set_percent = 0;
      new_fre_with_two_steps_set_percent = 0;
      new_fre_default_percent = 100;
      break;
  }

  // Set up the trial and groups.
  FirstRunFieldTrialConfig config(
      signin::kNewMobileIdentityConsistencyFRE.name);

  config.AddGroup(kNewMICEFREWithUMADialogSetGroup,
                  kNewMICEFREWithUMADialogSetID,
                  new_fre_with_uma_dialog_set_percent);

  config.AddGroup(kNewMICEFREWithThreeStepsSetGroup,
                  kNewMICEFREWithThreeStepsSetID,
                  new_fre_with_three_steps_set_percent);

  config.AddGroup(kNewMICEFREWithTwoStepsSetGroup, kNewMICEFREWithTwoStepsSetID,
                  new_fre_with_two_steps_set_percent);

  config.AddGroup(kDisabledGroup, kDisabledTrialID, new_fre_disabled_percent);
  config.AddGroup(kDefaultGroup, kDefaultTrialID, new_fre_default_percent);

  DCHECK_EQ(100, config.GetTotalProbability());

  // Associate field trial params to each group.
  AssociateFieldTrialParamsForNewMICEFREGroup(
      kNewMICEFREWithUMADialogSetGroup,
      kNewMobileIdentityConsistencyFREParamUMADialog);
  AssociateFieldTrialParamsForNewMICEFREGroup(
      kNewMICEFREWithThreeStepsSetGroup,
      kNewMobileIdentityConsistencyFREParamThreeSteps);
  AssociateFieldTrialParamsForNewMICEFREGroup(
      kNewMICEFREWithTwoStepsSetGroup,
      kNewMobileIdentityConsistencyFREParamTwoSteps);

  scoped_refptr<base::FieldTrial> trial =
      config.CreateOneTimeRandomizedTrial(kDefaultGroup, low_entropy_provider);

  // Finalize the group choice and activates the trial - similar to a variation
  // config that's marked with |starts_active| true. This is required for
  // studies that register variation ids, so they don't reveal extra information
  // beyond the low-entropy source.
  int group = trial->group();
  const std::string& group_name = trial->group_name();
  if (group_name == kNewMICEFREWithUMADialogSetGroup ||
      group_name == kNewMICEFREWithThreeStepsSetGroup ||
      group_name == kNewMICEFREWithTwoStepsSetGroup) {
    feature_list->RegisterFieldTrialOverride(
        signin::kNewMobileIdentityConsistencyFRE.name,
        base::FeatureList::OVERRIDE_ENABLE_FEATURE, trial.get());
  } else if (group_name == kDisabledGroup) {
    feature_list->RegisterFieldTrialOverride(
        signin::kNewMobileIdentityConsistencyFRE.name,
        base::FeatureList::OVERRIDE_DISABLE_FEATURE, trial.get());
  }
  return group;
}

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {}

void Create(const base::FieldTrial::EntropyProvider& low_entropy_provider,
            base::FeatureList* feature_list,
            PrefService* local_state) {
  return;
}

}  // namespace fre_field_trial
