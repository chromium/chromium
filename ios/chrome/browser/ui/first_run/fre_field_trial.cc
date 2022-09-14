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
#include "ios/chrome/browser/ui/first_run/ios_first_run_field_trials.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"

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
const char kControlGroup[] = "Control-V2";
// Group names for the default browser promo trial.
const char kFREDefaultBrowserAndSmallDelayBeforeOtherPromosGroup[] =
    "FREDefaultBrowserAndSmallDelayBeforeOtherPromos-V2";
const char kFREDefaultBrowserAndDefaultDelayBeforeOtherPromosGroup[] =
    "FREDefaultBrowserAndDefaultDelayBeforeOtherPromos-V2";
const char kFREDefaultBrowserPromoAtFirstRunOnlyGroup[] =
    "FREDefaultBrowserPromoAtFirstRunOnly-V2";

// Experiment IDs defined for the above field trial groups.
const variations::VariationID kControlTrialID = 3348210;
const variations::VariationID kFREDefaultBrowserPromoAtFirstRunOnlyID = 3348842;
const variations::VariationID
    kFREDefaultBrowserAndDefaultDelayBeforeOtherPromosID = 3348843;
const variations::VariationID
    kFREDefaultBrowserAndSmallDelayBeforeOtherPromosID = 3348844;

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
void AssociateFieldTrialParamsForDefaultBrowserGroup(
    const std::string& group_name,
    const std::string& value) {
  base::FieldTrialParams params;
  params[kFREDefaultBrowserPromoParam] = value;
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

// Returns the weight for each trial group according to the FRE variations.
std::map<variations::VariationID, int> GetGroupWeightsForFREVariations() {
  // It would probably be more efficient to use a fixed_flat_map.
  std::map<variations::VariationID, int> weight_by_id = {
      {kControlTrialID, 25},
      {kFREDefaultBrowserAndDefaultDelayBeforeOtherPromosID, 25},
      {kFREDefaultBrowserAndSmallDelayBeforeOtherPromosID, 25},
      {kFREDefaultBrowserPromoAtFirstRunOnlyID, 25}};
  return weight_by_id;
}

// Creates the trial config, initializes the trial that puts clients into
// different groups, and returns the version number of the current trial. There
// are 3 groups other than the default group:
// - FRE default browser promo: show 14 days after first run
// - FRE default browser promo: show 3 days after first run
// - FRE default browser promo: only on first run
int CreateNewMICeAndDefaultBrowserFRETrial(
    const std::map<variations::VariationID, int>& weight_by_id,
    const base::FieldTrial::EntropyProvider& low_entropy_provider,
    base::FeatureList* feature_list) {
  // Set up the trial and groups.
  FirstRunFieldTrialConfig config(kIOSMICeAndDefaultBrowserTrialName);

  // Control group.
  AddGroupToConfig(kControlGroup, kControlTrialID, weight_by_id, config);
  // Default browser promo experiment groups. (New FRE with MICe disabled.)
  AddGroupToConfig(kFREDefaultBrowserAndDefaultDelayBeforeOtherPromosGroup,
                   kFREDefaultBrowserAndDefaultDelayBeforeOtherPromosID,
                   weight_by_id, config);
  AddGroupToConfig(kFREDefaultBrowserAndSmallDelayBeforeOtherPromosGroup,
                   kFREDefaultBrowserAndSmallDelayBeforeOtherPromosID,
                   weight_by_id, config);
  AddGroupToConfig(kFREDefaultBrowserPromoAtFirstRunOnlyGroup,
                   kFREDefaultBrowserPromoAtFirstRunOnlyID, weight_by_id,
                   config);

  // Associate field trial params to each group.
  AssociateFieldTrialParamsForDefaultBrowserGroup(
      kFREDefaultBrowserAndDefaultDelayBeforeOtherPromosGroup,
      kFREDefaultBrowserPromoDefaultDelayParam);
  AssociateFieldTrialParamsForDefaultBrowserGroup(
      kFREDefaultBrowserAndSmallDelayBeforeOtherPromosGroup,
      kFREDefaultBrowserPromoShortDelayParam);
  AssociateFieldTrialParamsForDefaultBrowserGroup(
      kFREDefaultBrowserPromoAtFirstRunOnlyGroup,
      kFREDefaultBrowserPromoFirstRunOnlyParam);

  scoped_refptr<base::FieldTrial> trial = config.CreateOneTimeRandomizedTrial(
      /*default_group_name=*/kDefaultGroup, low_entropy_provider);

  // Finalize the group choice and activate the trial - similar to a variation
  // config that's marked with `starts_active` true. This is required for
  // studies that register variation ids, so they don't reveal extra information
  // beyond the low-entropy source.
  base::FeatureList::OverrideState state =
      trial->group_name() == kControlGroup
          ? base::FeatureList::OVERRIDE_DISABLE_FEATURE
          : base::FeatureList::OVERRIDE_ENABLE_FEATURE;
  feature_list->RegisterFieldTrialOverride(
      kEnableFREDefaultBrowserPromoScreen.name, state, trial.get());
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
          kEnableFREDefaultBrowserPromoScreen.name)) {
    return;
  }
  const std::map<variations::VariationID, int> weight_by_id =
      GetGroupWeightsForFREVariations();
  if (FirstRun::IsChromeFirstRun()) {
    // Create trial and group for the first time, and store the experiment
    // version in prefs for subsequent runs.
    int trial_version = CreateNewMICeAndDefaultBrowserFRETrial(
        weight_by_id, low_entropy_provider, feature_list);
    local_state->SetInteger(kTrialGroupMICeAndDefaultBrowserVersionPrefName,
                            trial_version);
  } else if (local_state->GetInteger(
                 kTrialGroupMICeAndDefaultBrowserVersionPrefName) ==
             kCurrentTrialVersion) {
    // The client was enrolled in this version of the experiment and was
    // assigned to a group in a previous run, and should be kept in the same
    // group.
    CreateNewMICeAndDefaultBrowserFRETrial(weight_by_id, low_entropy_provider,
                                           feature_list);
  }
}

int testing::CreateNewMICeAndDefaultBrowserFRETrialForTesting(
    const std::map<variations::VariationID, int>& weight_by_id,
    const base::FieldTrial::EntropyProvider& low_entropy_provider,
    base::FeatureList* feature_list) {
  return CreateNewMICeAndDefaultBrowserFRETrial(
      weight_by_id, low_entropy_provider, feature_list);
}

}  // namespace fre_field_trial
