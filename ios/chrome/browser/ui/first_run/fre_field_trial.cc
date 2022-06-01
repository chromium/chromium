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

const char kIOSMICeAndDefaultBrowserTrialName[] =
    "IOSTrialMICeAndDefaultBrowser";

// Parameters for new Default Browser Promo FRE.
const char kFREDefaultBrowserPromoParam[] = "variant_default_browser";
const char kFREDefaultBrowserPromoDefaultDelayParam[] =
    "variant_default_delay_enabled";
const char kFREDefaultBrowserPromoFirstRunOnlyParam[] =
    "variant_fre_only_enabled";
const char kFREDefaultBrowserPromoShortDelayParam[] =
    "variant_short_delay_enabled";

// Parameters for new Mobile Identity Consistency FRE.
const char kNewMobileIdentityConsistencyFREParam[] = "variant_new_mice_fre";
const char kNewMobileIdentityConsistencyFREParamUMADialog[] = "umadialog";
const char kNewMobileIdentityConsistencyFREParamThreeSteps[] = "3steps";
const char kNewMobileIdentityConsistencyFREParamTwoSteps[] = "2steps";

namespace {

// Store local state preference with whether the client has participated in
// IOSTrialMICeAndDefaultBrowser experiment or not.
const char kTrialGroupMICeAndDefaultBrowserVersionPrefName[] =
    "fre_refactoring_mice_and_default_browser.trial_version";
// The placeholder trial version that is stored for a client who has not been
// enrolled in the experiment.
const int kPlaceholderTrialVersion = -1;
// The current trial version; should be updated when the experiment is modified.
const int kCurrentTrialVersion = 1;

// Group names for the FRE redesign permissions trial.
const char kDefaultGroup[] = "Default";
// Group name for the FRE control group.
const char kControlGroup[] = "Control-V1";
// Group name for the FRE holdback group. This group holds back clients from the
// kEnableFREUIModuleIOS behavior, which was launched in M103.
const char kHoldbackGroup[] = "Disabled-V1";
// Group names for the default browser promo trial.
const char kFREDefaultBrowserAndSmallDelayBeforeOtherPromosGroup[] =
    "FREDefaultBrowserAndSmallDelayBeforeOtherPromos-V1";
const char kFREDefaultBrowserAndDefaultDelayBeforeOtherPromosGroup[] =
    "FREDefaultBrowserAndDefaultDelayBeforeOtherPromos-V1";
const char kDefaultBrowserPromoAtFirstRunOnlyGroup[] =
    "DefaultBrowserPromoAtFirstRunOnly-V1";
// Group names for the new Mobile Identity Consistency FRE.
const char kNewMICEFREWithUMADialogSetGroup[] =
    "NewMobileIdentityConsistencyFREParamUMADialog-V1";
const char kNewMICEFREWithThreeStepsSetGroup[] =
    "NewMobileIdentityConsistencyFREParamThreeSteps-V1";
const char kNewMICEFREWithTwoStepsSetGroup[] =
    "NewMobileIdentityConsistencyFREParamTwoSteps-V1";

// Experiment IDs defined for the above field trial groups.
const variations::VariationID kControlTrialID = 3348210;
const variations::VariationID kHoldbackTrialID = 3348217;
const variations::VariationID kDefaultBrowserPromoAtFirstRunOnlyID = 3348842;
const variations::VariationID
    kFREDefaultBrowserAndDefaultDelayBeforeOtherPromosID = 3348843;
const variations::VariationID
    kFREDefaultBrowserAndSmallDelayBeforeOtherPromosID = 3348844;
const variations::VariationID kNewMICEFREWithUMADialogSetID = 3348845;
const variations::VariationID kNewMICEFREWithThreeStepsSetID = 3348846;
const variations::VariationID kNewMICEFREWithTwoStepsSetID = 3348847;

// Options for kNewDefaultBrowserPromoFREParam.
constexpr base::FeatureParam<NewDefaultBrowserPromoFRE>::Option
    kNewDefaultBrowserPromoFREOptions[] = {
        {NewDefaultBrowserPromoFRE::kDefaultDelay,
         kFREDefaultBrowserPromoDefaultDelayParam},
        {NewDefaultBrowserPromoFRE::kFirstRunOnly,
         kFREDefaultBrowserPromoFirstRunOnlyParam},
        {NewDefaultBrowserPromoFRE::kShortDelay,
         kFREDefaultBrowserPromoShortDelayParam}};

// Parameter for kEnableFREDefaultBrowserPromoScreen feature.
constexpr base::FeatureParam<NewDefaultBrowserPromoFRE>
    kNewDefaultBrowserPromoFREParam{&kEnableFREDefaultBrowserPromoScreen,
                                    kFREDefaultBrowserPromoParam,
                                    NewDefaultBrowserPromoFRE::kDefaultDelay,
                                    &kNewDefaultBrowserPromoFREOptions};

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

// Sets the parameter value of the new default browser parameter.
void AssociateFieldTrialParamsForDefaultBrowserGroup(
    const std::string& group_name,
    const std::string& value) {
  base::FieldTrialParams params;
  params[kFREDefaultBrowserPromoParam] = value;
  bool association_result = base::AssociateFieldTrialParams(
      kIOSMICeAndDefaultBrowserTrialName, group_name, params);
  DCHECK(association_result);
}

// Sets the parameter value of the new MICE FRE parameter.
void AssociateFieldTrialParamsForNewMICEFREGroup(const std::string& group_name,
                                                 const std::string& value) {
  base::FieldTrialParams params;
  params[kNewMobileIdentityConsistencyFREParam] = value;
  bool association_result = base::AssociateFieldTrialParams(
      kIOSMICeAndDefaultBrowserTrialName, group_name, params);
  DCHECK(association_result);
}

}  // namespace

namespace fre_field_trial {

NewDefaultBrowserPromoFRE GetFREDefaultBrowserScreenPromoFRE() {
  if (base::FeatureList::IsEnabled(kEnableFREUIModuleIOS) &&
      base::FeatureList::IsEnabled(kEnableFREDefaultBrowserPromoScreen)) {
    return kNewDefaultBrowserPromoFREParam.Get();
  }
  return NewDefaultBrowserPromoFRE::kDisabled;
}

NewMobileIdentityConsistencyFRE GetNewMobileIdentityConsistencyFRE() {
  if (base::FeatureList::IsEnabled(signin::kNewMobileIdentityConsistencyFRE)) {
    return kkNewMobileIdentityConsistencyFREParam.Get();
  }
  return NewMobileIdentityConsistencyFRE::kOld;
}

// Creates the trial config, initializes the trial that puts clients into
// different groups, and returns the version number of the current trial. There
// are 9 groups:
// - Default
// - Control
// - Holdback
// - New MICE FRE with UMA dialog
// - New MICE FRE with 3 steps
// - New MICE FRE with 2 steps
// - FRE default browser promo: show 14 days after first run
// - FRE default browser promo: show 3 days after first run
// - FRE default browser promo: only on first run
int CreateNewMICeAndDefaultBrowserFRETrial(
    const base::FieldTrial::EntropyProvider& low_entropy_provider,
    base::FeatureList* feature_list) {
  // Experiment groups
  int new_fre_control_percent = 0;
  int new_fre_holdback_percent = 0;
  // MICe FRE experiment.
  int new_fre_with_uma_dialog_set_percent = 0;
  int new_fre_with_three_steps_set_percent = 0;
  int new_fre_with_two_steps_set_percent = 0;
  // FRE's default browser screen experiment
  int new_fre_with_default_screen_and_default_cooldown_percent = 0;
  int new_fre_with_default_screen_and_short_cooldown_percent = 0;
  int new_fre_with_default_screen_only_percent = 0;

  switch (GetChannel()) {
    case version_info::Channel::UNKNOWN:
    case version_info::Channel::CANARY:
    case version_info::Channel::DEV:
    case version_info::Channel::BETA:
      new_fre_control_percent = 10;
      new_fre_holdback_percent = 10;
      new_fre_with_uma_dialog_set_percent = 10;
      new_fre_with_three_steps_set_percent = 10;
      new_fre_with_two_steps_set_percent = 10;
      new_fre_with_default_screen_and_default_cooldown_percent = 10;
      new_fre_with_default_screen_and_short_cooldown_percent = 10;
      new_fre_with_default_screen_only_percent = 10;
      break;
    case version_info::Channel::STABLE:
      new_fre_control_percent = 8;
      new_fre_holdback_percent = 8;
      new_fre_with_uma_dialog_set_percent = 8;
      new_fre_with_three_steps_set_percent = 8;
      new_fre_with_two_steps_set_percent = 8;
      new_fre_with_default_screen_and_default_cooldown_percent = 8;
      new_fre_with_default_screen_and_short_cooldown_percent = 8;
      new_fre_with_default_screen_only_percent = 8;
      break;
  }

  // Set up the trial and groups.
  FirstRunFieldTrialConfig config(kIOSMICeAndDefaultBrowserTrialName);
  // Disabled and control groups.
  config.AddGroup(kControlGroup, kControlTrialID, new_fre_control_percent);
  config.AddGroup(kHoldbackGroup, kHoldbackTrialID, new_fre_holdback_percent);
  // MICe experiment groups. (No default browser promo.)
  config.AddGroup(kNewMICEFREWithUMADialogSetGroup,
                  kNewMICEFREWithUMADialogSetID,
                  new_fre_with_uma_dialog_set_percent);
  config.AddGroup(kNewMICEFREWithThreeStepsSetGroup,
                  kNewMICEFREWithThreeStepsSetID,
                  new_fre_with_three_steps_set_percent);
  config.AddGroup(kNewMICEFREWithTwoStepsSetGroup, kNewMICEFREWithTwoStepsSetID,
                  new_fre_with_two_steps_set_percent);
  // Default browser promo experiment groups. (New FRE with MICe disabled.)
  config.AddGroup(kFREDefaultBrowserAndDefaultDelayBeforeOtherPromosGroup,
                  kFREDefaultBrowserAndDefaultDelayBeforeOtherPromosID,
                  new_fre_with_default_screen_and_default_cooldown_percent);
  config.AddGroup(kFREDefaultBrowserAndSmallDelayBeforeOtherPromosGroup,
                  kFREDefaultBrowserAndSmallDelayBeforeOtherPromosID,
                  new_fre_with_default_screen_and_short_cooldown_percent);
  config.AddGroup(kDefaultBrowserPromoAtFirstRunOnlyGroup,
                  kDefaultBrowserPromoAtFirstRunOnlyID,
                  new_fre_with_default_screen_only_percent);

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
  AssociateFieldTrialParamsForDefaultBrowserGroup(
      kFREDefaultBrowserAndDefaultDelayBeforeOtherPromosGroup,
      kFREDefaultBrowserPromoDefaultDelayParam);
  AssociateFieldTrialParamsForDefaultBrowserGroup(
      kFREDefaultBrowserAndSmallDelayBeforeOtherPromosGroup,
      kFREDefaultBrowserPromoShortDelayParam);
  AssociateFieldTrialParamsForDefaultBrowserGroup(
      kDefaultBrowserPromoAtFirstRunOnlyGroup,
      kFREDefaultBrowserPromoFirstRunOnlyParam);

  scoped_refptr<base::FieldTrial> trial = config.CreateOneTimeRandomizedTrial(
      /*default_group_name=*/kDefaultGroup, low_entropy_provider);

  // Finalize the group choice and activate the trial - similar to a variation
  // config that's marked with |starts_active| true. This is required for
  // studies that register variation ids, so they don't reveal extra information
  // beyond the low-entropy source.
  const std::string& group_name = trial->group_name();
  if (group_name == kDefaultGroup) {
    // Default behavior should be expected for default group; no configuration
    // to override any feature status is needed.
    return kCurrentTrialVersion;
  }
  if (group_name == kControlGroup) {
    // Explicitly set the features to align with the default group behavior in
    // case the default behavior of any experiment group changes before the
    // experiment reaches an end.
    feature_list->RegisterFieldTrialOverride(
        kEnableFREUIModuleIOS.name, base::FeatureList::OVERRIDE_ENABLE_FEATURE,
        trial.get());
    feature_list->RegisterFieldTrialOverride(
        signin::kNewMobileIdentityConsistencyFRE.name,
        base::FeatureList::OVERRIDE_DISABLE_FEATURE, trial.get());
    feature_list->RegisterFieldTrialOverride(
        kEnableFREDefaultBrowserPromoScreen.name,
        base::FeatureList::OVERRIDE_DISABLE_FEATURE, trial.get());
    return kCurrentTrialVersion;
  }
  if (group_name == kHoldbackGroup) {
    feature_list->RegisterFieldTrialOverride(
        kEnableFREUIModuleIOS.name, base::FeatureList::OVERRIDE_DISABLE_FEATURE,
        trial.get());
    feature_list->RegisterFieldTrialOverride(
        signin::kNewMobileIdentityConsistencyFRE.name,
        base::FeatureList::OVERRIDE_DISABLE_FEATURE, trial.get());
    feature_list->RegisterFieldTrialOverride(
        kEnableFREDefaultBrowserPromoScreen.name,
        base::FeatureList::OVERRIDE_DISABLE_FEATURE, trial.get());
    return kCurrentTrialVersion;
  }
  feature_list->RegisterFieldTrialOverride(
      kEnableFREUIModuleIOS.name, base::FeatureList::OVERRIDE_ENABLE_FEATURE,
      trial.get());
  if (group_name == kFREDefaultBrowserAndSmallDelayBeforeOtherPromosGroup ||
      group_name == kFREDefaultBrowserAndDefaultDelayBeforeOtherPromosGroup ||
      group_name == kDefaultBrowserPromoAtFirstRunOnlyGroup) {
    feature_list->RegisterFieldTrialOverride(
        signin::kNewMobileIdentityConsistencyFRE.name,
        base::FeatureList::OVERRIDE_DISABLE_FEATURE, trial.get());
    feature_list->RegisterFieldTrialOverride(
        kEnableFREDefaultBrowserPromoScreen.name,
        base::FeatureList::OVERRIDE_ENABLE_FEATURE, trial.get());
    return kCurrentTrialVersion;
  }
  if (group_name == kNewMICEFREWithUMADialogSetGroup ||
      group_name == kNewMICEFREWithThreeStepsSetGroup ||
      group_name == kNewMICEFREWithTwoStepsSetGroup) {
    feature_list->RegisterFieldTrialOverride(
        signin::kNewMobileIdentityConsistencyFRE.name,
        base::FeatureList::OVERRIDE_ENABLE_FEATURE, trial.get());
    feature_list->RegisterFieldTrialOverride(
        kEnableFREDefaultBrowserPromoScreen.name,
        base::FeatureList::OVERRIDE_DISABLE_FEATURE, trial.get());
  }
  return kCurrentTrialVersion;
}

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kTrialGroupMICeAndDefaultBrowserVersionPrefName,
                                kPlaceholderTrialVersion);
}

void Create(const base::FieldTrial::EntropyProvider& low_entropy_provider,
            base::FeatureList* feature_list,
            PrefService* local_state) {
  // The client would not be assigned to any group because features controlled
  // by the experiment is already overridden from the command line. This handles
  // scenarios where FRE is forced for testing purposes.
  if (feature_list->IsFeatureOverriddenFromCommandLine(
          kEnableFREUIModuleIOS.name) ||
      feature_list->IsFeatureOverriddenFromCommandLine(
          kEnableFREDefaultBrowserPromoScreen.name) ||
      feature_list->IsFeatureOverriddenFromCommandLine(
          signin::kNewMobileIdentityConsistencyFRE.name)) {
    return;
  }
  if (FirstRun::IsChromeFirstRun()) {
    // Create trial and group for the first time, and store the experiment
    // version in prefs for subsequent runs.
    int trial_version = CreateNewMICeAndDefaultBrowserFRETrial(
        low_entropy_provider, feature_list);
    local_state->SetInteger(kTrialGroupMICeAndDefaultBrowserVersionPrefName,
                            trial_version);
  } else if (local_state->GetInteger(
                 kTrialGroupMICeAndDefaultBrowserVersionPrefName) ==
             kCurrentTrialVersion) {
    // The client was enrolled in this version of the experiment and was
    // assigned to a group in a previous run, and should be kept in the same
    // group.
    CreateNewMICeAndDefaultBrowserFRETrial(low_entropy_provider, feature_list);
  }
}

}  // namespace fre_field_trial
