// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/first_run/fre_field_trial.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_associated_data.h"
#include "components/version_info/version_info.h"
#include "ios/chrome/browser/first_run/first_run.h"
#include "ios/chrome/browser/ui/first_run/ios_first_run_field_trials.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/common/channel_info.h"

namespace {
// String local state preference with the name of the assigned trial group.
// Empty if no group has been assigned yet.
const char kTrialGroupPrefName[] = "fre_refactoring.trial_group";

// Group names for the FRE redesign permissions trial.
const char kEnabledGroup[] = "Enabled";
const char kDisabledGroup[] = "Disabled";
const char kDefaultGroup[] = "Default";
// Experiment IDs defined for the above field trial groups.
const variations::VariationID kDefaultTrialID = 3330131;
const variations::VariationID kEnabledTrialID = 3330132;
const variations::VariationID kDisabledTrialID = 3330133;

// Default local state pref value.
const int kDefaultPrefValue = -1;
}  // namespace

namespace fre_field_trial {

// Creates a trial for the first run (when there is no variations seed) if
// necessary and enables the feature based on the randomly selected trial group.
// Returns the group number.
int CreateFirstRunTrial(
    base::FieldTrial::EntropyProvider const& low_entropy_provider,
    base::FeatureList* feature_list) {
  int enabled_percent;
  int control_percent;
  int default_percent;
  switch (GetChannel()) {
    case version_info::Channel::UNKNOWN:
    case version_info::Channel::CANARY:
    case version_info::Channel::DEV:
    case version_info::Channel::BETA:
      enabled_percent = 0;
      control_percent = 0;
      default_percent = 100;
      break;
    case version_info::Channel::STABLE:
      enabled_percent = 0;
      control_percent = 0;
      default_percent = 100;
      break;
  }

  // Set up the trial and groups.
  FirstRunFieldTrialConfig config(kEnableFREUIModuleIOS.name);

  config.AddGroup(kDefaultGroup, kDefaultTrialID, default_percent);
  config.AddGroup(kEnabledGroup, kEnabledTrialID, enabled_percent);
  config.AddGroup(kDisabledGroup, kDisabledTrialID, control_percent);
  DCHECK_EQ(100, config.GetTotalProbability());

  scoped_refptr<base::FieldTrial> trial =
      config.CreateOneTimeRandomizedTrial(kDefaultGroup, low_entropy_provider);

  // Finalize the group choice and activates the trial - similar to a variation
  // config that's marked with |starts_active| true. This is required for
  // studies that register variation ids, so they don't reveal extra information
  // beyond the low-entropy source.
  int group = trial->group();
  base::FeatureList::OverrideState feature_state =
      trial->group_name() == kEnabledGroup
          ? base::FeatureList::OVERRIDE_ENABLE_FEATURE
          : base::FeatureList::OVERRIDE_DISABLE_FEATURE;

  feature_list->RegisterFieldTrialOverride(kEnableFREUIModuleIOS.name,
                                           feature_state, trial.get());
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

}  // namespace fre_field_trial
