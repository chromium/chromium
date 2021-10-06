// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/first_run/default_browser_promo_field_trial.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_associated_data.h"
#include "components/version_info/version_info.h"
#include "ios/chrome/browser/first_run/first_run.h"
#include "ios/chrome/browser/ui/first_run/ios_first_run_field_trials.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/common/channel_info.h"

const char kFREDefaultPromoTestingDefaultDelayParam[] =
    "variant_default_delay_enabled";
const char kFREDefaultPromoTestingFREOnlyParam[] = "variant_fre_only_enabled";
const char kFREDefaultPromoTestingShortDelayParam[] =
    "variant_short_delay_enabled";

namespace {
// String local state preference with the name of the assigned trial group.
// Empty if no group has been assigned yet.
const char kTrialGroupPrefName[] = "fre_default_browser_promo.trial_group";

// Group names for the default browser promo trial.
const char kFREDefaultBrowserAndSmallDelayBeforeOtherPromosGroup[] =
    "FREDefaultBrowserAndSmallDelayBeforeOtherPromos";
const char kFREDefaultBrowserAndDefaultDelayBeforeOtherPromosGroup[] =
    "FREDefaultBrowserAndDefaultDelayBeforeOtherPromos";
const char kDefaultBrowserPromoAtFirstRunOnlyGroup[] =
    "DefaultBrowserPromoAtFirstRunOnly";
const char kDisabledGroup[] = "Disabled";
const char kDefaultGroup[] = "Default";
// Experiment IDs defined for the above field trial groups.
const variations::VariationID kDefaultTrialID = 3342134;
const variations::VariationID kDisabledTrialID = 3342135;
const variations::VariationID kDefaultBrowserPromoAtFirstRunOnlyID = 3342136;
const variations::VariationID
    kFREDefaultBrowserAndDefaultDelayBeforeOtherPromosID = 3342137;
const variations::VariationID
    kFREDefaultBrowserAndSmallDelayBeforeOtherPromosID = 3342138;

// Default local state pref value.
const int kDefaultPrefValue = -1;
}  // namespace

namespace fre_default_browser_promo_field_trial {

// Sets the feature state based on the trial group. Defaults to disabled.
void SetFeatureState(const std::string& group_name,
                     base::FeatureList* feature_list,
                     base::FieldTrial* trial) {
  base::FeatureList::OverrideState feature_state =
      base::FeatureList::OVERRIDE_DISABLE_FEATURE;
  if (group_name == kFREDefaultBrowserAndSmallDelayBeforeOtherPromosGroup ||
      group_name == kFREDefaultBrowserAndDefaultDelayBeforeOtherPromosGroup ||
      group_name == kDefaultBrowserPromoAtFirstRunOnlyGroup) {
    feature_state = base::FeatureList::OVERRIDE_ENABLE_FEATURE;
  }

  feature_list->RegisterFieldTrialOverride(kEnableFREDefaultBrowserScreen.name,
                                           feature_state, trial);
}

bool IsInFirstRunDefaultBrowserAndSmallDelayBeforeOtherPromosGroup() {
  if (base::FeatureList::IsEnabled(kEnableFREDefaultBrowserScreenTesting)) {
    return base::GetFieldTrialParamByFeatureAsBool(
        kEnableFREDefaultBrowserScreenTesting,
        kFREDefaultPromoTestingShortDelayParam, false);
  }
  return base::FeatureList::IsEnabled(kEnableFREDefaultBrowserScreen) &&
         base::FieldTrialList::FindFullName(
             kEnableFREDefaultBrowserScreen.name) ==
             kFREDefaultBrowserAndSmallDelayBeforeOtherPromosGroup;
}

bool IsInFirstRunDefaultBrowserAndDefaultDelayBeforeOtherPromosGroup() {
  if (base::FeatureList::IsEnabled(kEnableFREDefaultBrowserScreenTesting)) {
    return base::GetFieldTrialParamByFeatureAsBool(
        kEnableFREDefaultBrowserScreenTesting,
        kFREDefaultPromoTestingDefaultDelayParam, false);
  }
  return base::FeatureList::IsEnabled(kEnableFREDefaultBrowserScreen) &&
         base::FieldTrialList::FindFullName(
             kEnableFREDefaultBrowserScreen.name) ==
             kFREDefaultBrowserAndDefaultDelayBeforeOtherPromosGroup;
}

bool IsInDefaultBrowserPromoAtFirstRunOnlyGroup() {
  if (base::FeatureList::IsEnabled(kEnableFREDefaultBrowserScreenTesting)) {
    return base::GetFieldTrialParamByFeatureAsBool(
        kEnableFREDefaultBrowserScreenTesting,
        kFREDefaultPromoTestingFREOnlyParam, false);
  }
  return base::FeatureList::IsEnabled(kEnableFREDefaultBrowserScreen) &&
         base::FieldTrialList::FindFullName(
             kEnableFREDefaultBrowserScreen.name) ==
             kDefaultBrowserPromoAtFirstRunOnlyGroup;
}

bool IsFREDefaultBrowserScreenEnabled() {
  return IsInFirstRunDefaultBrowserAndSmallDelayBeforeOtherPromosGroup() ||
         IsInFirstRunDefaultBrowserAndDefaultDelayBeforeOtherPromosGroup() ||
         IsInDefaultBrowserPromoAtFirstRunOnlyGroup();
}

// Creates a trial for the first run (when there is no variations seed) if
// necessary and enables the feature based on the randomly selected trial group.
// Returns the group number.
int CreateFirstRunTrial(
    base::FieldTrial::EntropyProvider const& low_entropy_provider,
    base::FeatureList* feature_list) {
  int fre_and_short_cooldown_percent = 0;
  int fre_and_default_cooldown_percent = 0;
  int fre_only_percent = 0;
  int disabled_percent = 0;
  int default_percent = 0;
  switch (GetChannel()) {
    case version_info::Channel::UNKNOWN:
    case version_info::Channel::CANARY:
    case version_info::Channel::DEV:
    case version_info::Channel::BETA:
      fre_and_short_cooldown_percent = 0;
      fre_and_default_cooldown_percent = 0;
      fre_only_percent = 0;
      disabled_percent = 0;
      default_percent = 100;
      break;
    case version_info::Channel::STABLE:
      fre_and_short_cooldown_percent = 0;
      fre_and_default_cooldown_percent = 0;
      fre_only_percent = 0;
      disabled_percent = 0;
      default_percent = 100;
      break;
  }

  // Set up the trial and groups.
  FirstRunFieldTrialConfig config(kEnableFREDefaultBrowserScreen.name);
  config.AddGroup(kDefaultGroup, kDefaultTrialID, default_percent);

  config.AddGroup(kFREDefaultBrowserAndSmallDelayBeforeOtherPromosGroup,
                  kFREDefaultBrowserAndSmallDelayBeforeOtherPromosID,
                  fre_and_short_cooldown_percent);
  config.AddGroup(kFREDefaultBrowserAndDefaultDelayBeforeOtherPromosGroup,
                  kFREDefaultBrowserAndDefaultDelayBeforeOtherPromosID,
                  fre_and_default_cooldown_percent);
  config.AddGroup(kDefaultBrowserPromoAtFirstRunOnlyGroup,
                  kDefaultBrowserPromoAtFirstRunOnlyID, fre_only_percent);

  // Add disabled group last to match internal implementation in field_trial.cc
  config.AddGroup(kDisabledGroup, kDisabledTrialID, disabled_percent);
  DCHECK_EQ(100, config.GetTotalProbability());
  scoped_refptr<base::FieldTrial> trial =
      config.CreateOneTimeRandomizedTrial(kDisabledGroup, low_entropy_provider);

  // Finalize the group choice and activates the trial - similar to a variation
  // config that's marked with |starts_active| true. This is required for
  // studies that register variation ids, so they don't reveal extra information
  // beyond the low-entropy source.
  int group = trial->group();
  const std::string& group_name = trial->group_name();
  SetFeatureState(group_name, feature_list, trial.get());
  return group;
}

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kTrialGroupPrefName, kDefaultPrefValue);
}

void Create(const base::FieldTrial::EntropyProvider& low_entropy_provider,
            base::FeatureList* feature_list,
            PrefService* local_state) {
  int trial_group = local_state->GetInteger(kTrialGroupPrefName);
  if (trial_group == kDefaultPrefValue && !FirstRun::IsChromeFirstRun()) {
    // Do not bucket existing users that have not been already been grouped. The
    // experiment wants to only add users who have not seen the First Run
    // Experience yet.
    return;
  }
  // Create trial and group user for the first time, or tag users again to
  // ensure the experiment can be used to filter UMA metrics.
  trial_group = CreateFirstRunTrial(low_entropy_provider, feature_list);
  // Persist the assigned group for subsequent runs.
  local_state->SetInteger(kTrialGroupPrefName, trial_group);
}

}  // namespace fre_default_browser_promo_field_trial
