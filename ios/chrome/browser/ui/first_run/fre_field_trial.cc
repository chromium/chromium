// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/first_run/fre_field_trial.h"

#include <map>

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/signin/ios/browser/features.h"
#include "components/version_info/version_info.h"
#include "ios/chrome/browser/first_run/first_run.h"
#import "ios/chrome/browser/ui/first_run/field_trial_constants.h"
#import "ios/chrome/browser/ui/first_run/field_trial_ids.h"
#include "ios/chrome/browser/ui/first_run/ios_first_run_field_trials.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/common/channel_info.h"

namespace {

// Store local state preference with whether the client has participated in
// `x`kIOSMICeAndDefaultBrowserTrialName` experiment or not.
const char kTrialGroupMICeAndDefaultBrowserVersionPrefName[] =
    "fre_refactoring_mice_and_default_browser.trial_version";
// The placeholder trial version that is stored for a client who has not been
// enrolled in the experiment.
const int kPlaceholderTrialVersion = -1;
// The current trial version; should be updated when the experiment is modified.
const int kCurrentTrialVersion = 5;

// Group names for the FRE redesign permissions trial.
const char kDefaultGroup[] = "Default";
// Group name for the FRE control group.
const char kControlGroup[] = "Control-V5";
// Group names for the MICe FRE and TangibleSync FRE trial.
const char kTangibleSyncAFREGroup[] = "kTangibleSyncA-V5";
const char kTangibleSyncDFREGroup[] = "kTangibleSyncD-V5";
const char kTangibleSyncEFREGroup[] = "kTangibleSyncE-V5";
const char kTangibleSyncFFREGroup[] = "kTangibleSyncF-V5";
const char kTwoStepsMICEFREGroup[] = "kTwoStepsMICEFRE-V5";

// Options for kkNewMobileIdentityConsistencyFREParam.
constexpr base::FeatureParam<NewMobileIdentityConsistencyFRE>::Option
    kNewMobileIdentityConsistencyFREOptions[] = {
        {NewMobileIdentityConsistencyFRE::kTangibleSyncA,
         kNewMobileIdentityConsistencyFREParamTangibleSyncA},
        {NewMobileIdentityConsistencyFRE::kTangibleSyncB,
         kNewMobileIdentityConsistencyFREParamTangibleSyncB},
        {NewMobileIdentityConsistencyFRE::kTangibleSyncC,
         kNewMobileIdentityConsistencyFREParamTangibleSyncC},
        {NewMobileIdentityConsistencyFRE::kTwoSteps,
         kNewMobileIdentityConsistencyFREParamTwoSteps},
        {NewMobileIdentityConsistencyFRE::kTangibleSyncD,
         kNewMobileIdentityConsistencyFREParamTangibleSyncD},
        {NewMobileIdentityConsistencyFRE::kTangibleSyncE,
         kNewMobileIdentityConsistencyFREParamTangibleSyncE},
        {NewMobileIdentityConsistencyFRE::kTangibleSyncF,
         kNewMobileIdentityConsistencyFREParamTangibleSyncF},
};

// Parameter for signin::kNewMobileIdentityConsistencyFRE feature.
constexpr base::FeatureParam<NewMobileIdentityConsistencyFRE>
    kkNewMobileIdentityConsistencyFREParam{
        &signin::kNewMobileIdentityConsistencyFRE,
        kNewMobileIdentityConsistencyFREParam,
        NewMobileIdentityConsistencyFRE::kTangibleSyncA,
        &kNewMobileIdentityConsistencyFREOptions};

// Adds a trial group to a FRE field trial config with the given group name,
// variation ID, and weight.
void AddGroupToConfig(
    const std::string& group_name,
    const variations::VariationID group_id,
    const std::map<variations::VariationID, int>& weight_by_id,
    FirstRunFieldTrialConfig& config) {
  auto it = weight_by_id.find(group_id);
  DCHECK(it != weight_by_id.end())
      << "Required variation ID missing: " << group_id;
  config.AddGroup(group_name, group_id, it->second);
}

// Sets the parameter value of the new default browser parameter.
void AssociateFieldTrialParamsForNewMobileIdentityConsistency(
    const std::string& group_name,
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
  if (base::FeatureList::IsEnabled(kEnableFREDefaultBrowserPromoScreen)) {
    return NewDefaultBrowserPromoFRE::kShortDelay;
  }
  return NewDefaultBrowserPromoFRE::kDisabled;
}

NewMobileIdentityConsistencyFRE GetNewMobileIdentityConsistencyFRE() {
  if (base::FeatureList::IsEnabled(signin::kNewMobileIdentityConsistencyFRE)) {
    return kkNewMobileIdentityConsistencyFREParam.Get();
  }
  return NewMobileIdentityConsistencyFRE::kOld;
}

// Returns the weight for each trial group according to the FRE variations.
std::map<variations::VariationID, int> GetGroupWeightsForFREVariations() {
  // It would probably be more efficient to use a fixed_flat_map.
  // kTangibleSyncAFRETrialID is launched to 100% of the users.
  std::map<variations::VariationID, int> weight_by_id = {
      {kControlTrialID, 0},          {kTangibleSyncAFRETrialID, 0},
      {kTangibleSyncDFRETrialID, 0}, {kTangibleSyncEFRETrialID, 0},
      {kTangibleSyncFFRETrialID, 0}, {kTwoStepsMICEFRETrialID, 0},
  };

  // `kTangibleSyncAFRETrialID` launched to 100% of users.
  weight_by_id[kTangibleSyncAFRETrialID] = 100;
  return weight_by_id;
}

// Creates the trial config, initializes the trial that puts clients into
// different groups, and returns the version number of the current trial. There
// are 5 groups other than the default group:
//  * Control group
//  * TangibleSync A FRE group.
//  * TangibleSync B FRE group.
//  * TangibleSync C FRE group.
//  * MICe FRE FRE group.
int CreateNewMICeFRETrial(
    const std::map<variations::VariationID, int>& weight_by_id,
    const base::FieldTrial::EntropyProvider& low_entropy_provider,
    base::FeatureList* feature_list) {
  // Set up the trial and groups.
  FirstRunFieldTrialConfig config(kIOSMICeAndDefaultBrowserTrialName);

  // Control group.
  AddGroupToConfig(kControlGroup, kControlTrialID, weight_by_id, config);

  // MICe FRE and TangibleSync FRE groups.
  AddGroupToConfig(kTangibleSyncAFREGroup, kTangibleSyncAFRETrialID,
                   weight_by_id, config);
  AddGroupToConfig(kTangibleSyncDFREGroup, kTangibleSyncDFRETrialID,
                   weight_by_id, config);
  AddGroupToConfig(kTangibleSyncEFREGroup, kTangibleSyncEFRETrialID,
                   weight_by_id, config);
  AddGroupToConfig(kTangibleSyncFFREGroup, kTangibleSyncFFRETrialID,
                   weight_by_id, config);
  AddGroupToConfig(kTwoStepsMICEFREGroup, kTwoStepsMICEFRETrialID, weight_by_id,
                   config);

  // Associate field trial params to each group.
  AssociateFieldTrialParamsForNewMobileIdentityConsistency(
      kTangibleSyncAFREGroup,
      kNewMobileIdentityConsistencyFREParamTangibleSyncA);
  AssociateFieldTrialParamsForNewMobileIdentityConsistency(
      kTangibleSyncDFREGroup,
      kNewMobileIdentityConsistencyFREParamTangibleSyncD);
  AssociateFieldTrialParamsForNewMobileIdentityConsistency(
      kTangibleSyncEFREGroup,
      kNewMobileIdentityConsistencyFREParamTangibleSyncE);
  AssociateFieldTrialParamsForNewMobileIdentityConsistency(
      kTangibleSyncFFREGroup,
      kNewMobileIdentityConsistencyFREParamTangibleSyncF);
  AssociateFieldTrialParamsForNewMobileIdentityConsistency(
      kTwoStepsMICEFREGroup, kNewMobileIdentityConsistencyFREParamTwoSteps);

  scoped_refptr<base::FieldTrial> trial = config.CreateOneTimeRandomizedTrial(
      /*default_group_name=*/kDefaultGroup, low_entropy_provider);

  // Finalize the group choice and activate the trial - similar to a variation
  // config that's marked with `starts_active` true. This is required for
  // studies that register variation ids, so they don't reveal extra information
  // beyond the low-entropy source.
  base::FeatureList::OverrideState state =
      ((trial->group_name() == kDefaultGroup) ||
       (trial->group_name() == kControlGroup))
          ? base::FeatureList::OVERRIDE_DISABLE_FEATURE
          : base::FeatureList::OVERRIDE_ENABLE_FEATURE;
  feature_list->RegisterFieldTrialOverride(
      signin::kNewMobileIdentityConsistencyFRE.name, state, trial.get());
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
  // by the experiment if the feature is already overridden. This handles
  // scenarios where FRE is forced for testing purposes.
  if (feature_list->IsFeatureOverridden(
          signin::kNewMobileIdentityConsistencyFRE.name)) {
    return;
  }
  const std::map<variations::VariationID, int> weight_by_id =
      GetGroupWeightsForFREVariations();
  int trial_version =
      local_state->GetInteger(kTrialGroupMICeAndDefaultBrowserVersionPrefName);
  if (FirstRun::IsChromeFirstRun() &&
      trial_version == kPlaceholderTrialVersion) {
    // Create trial and group for the first time, and store the experiment
    // version in prefs for subsequent runs.
    trial_version =
        CreateNewMICeFRETrial(weight_by_id, low_entropy_provider, feature_list);
    local_state->SetInteger(kTrialGroupMICeAndDefaultBrowserVersionPrefName,
                            trial_version);
  } else if (trial_version == kCurrentTrialVersion) {
    // The client was enrolled in this version of the experiment and was
    // assigned to a group in a previous run, and should be kept in the same
    // group.
    CreateNewMICeFRETrial(weight_by_id, low_entropy_provider, feature_list);
  }
}

int testing::CreateNewMICeAndDefaultBrowserFRETrialForTesting(
    const std::map<variations::VariationID, int>& weight_by_id,
    const base::FieldTrial::EntropyProvider& low_entropy_provider,
    base::FeatureList* feature_list) {
  return CreateNewMICeFRETrial(weight_by_id, low_entropy_provider,
                               feature_list);
}

}  // namespace fre_field_trial
