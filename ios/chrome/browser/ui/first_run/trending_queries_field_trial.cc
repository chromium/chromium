// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/trending_queries_field_trial.h"

#import "base/feature_list.h"
#import "base/metrics/field_trial.h"
#import "base/metrics/field_trial_params.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/first_run/first_run.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/first_run/ios_first_run_field_trials.h"
#import "ios/chrome/common/channel_info.h"

namespace {
// String local state preference with the name of the assigned trial group.
// Empty if no group has been assigned yet.
const char kTrialVersionPrefName[] = "trending_queries.trial_group";
// Default local state pref value.
const int kDefaultPrefValue = -1;
// The current trial version; should be updated when the experiment is modified.
const int kCurrentTrialVersion = 1;

// Group names for the Trending Queries feature.
const char kTrendingQueriesEnabledAllUsersGroup[] = "EnabledAllUsers-V1";
const char kTrendingQueriesEnabledAllUsersHideShortcutsGroup[] =
    "EnabledAllUsersHideShortcuts-V1";
const char kTrendingQueriesEnabledDisabledFeedGroup[] =
    "EnabledDisabledFeed-V1";
const char kTrendingQueriesEnabledSignedOutGroup[] = "EnabledSignedOut-V1";
const char kTrendingQueriesEnabledNeverShowModuleGroup[] =
    "EnabledNeverShowModule-V1";
const char kTrendingQueriesControlGroup[] = "Control-V1";
const char kTrendingQueriesDefaultGroup[] = "Default";

// Returns a map of the group weights for each arm.
std::map<variations::VariationID, int> GetGroupWeights() {
  std::map<variations::VariationID, int> weight_by_id = {
      {kTrendingQueriesEnabledAllUsersID, 0},
      {kTrendingQueriesEnabledAllUsersHideShortcutsID, 0},
      {kTrendingQueriesEnabledDisabledFeedID, 0},
      {kTrendingQueriesEnabledSignedOutID, 0},
      {kTrendingQueriesEnabledNeverShowModuleID, 0},
      {kTrendingQueriesControlID, 0}};
  switch (GetChannel()) {
    case version_info::Channel::UNKNOWN:
    case version_info::Channel::CANARY:
    case version_info::Channel::DEV:
    case version_info::Channel::BETA:
      weight_by_id[kTrendingQueriesEnabledAllUsersID] = 10;
      weight_by_id[kTrendingQueriesEnabledAllUsersHideShortcutsID] = 10;
      weight_by_id[kTrendingQueriesEnabledDisabledFeedID] = 10;
      weight_by_id[kTrendingQueriesEnabledSignedOutID] = 10;
      weight_by_id[kTrendingQueriesEnabledNeverShowModuleID] = 10;
      weight_by_id[kTrendingQueriesControlID] = 10;
      break;
    case version_info::Channel::STABLE:
      break;
  }
  return weight_by_id;
}

}  // namespace

namespace trending_queries_field_trial {

// Creates the trial config, initializes the trial that puts clients into
// different groups, and returns the version number of the current trial. There
// are 5 groups other than the default group:
// - Control
// - Enabled for All users
// - Enabled for All users and Hide Shortcuts
// - Enabled for signed out users
// - Enabled for users that had the feed disabled
int CreateTrendingQueriesTrial(
    std::map<variations::VariationID, int> weight_by_id,
    const base::FieldTrial::EntropyProvider& low_entropy_provider,
    base::FeatureList* feature_list) {
  FirstRunFieldTrialConfig config(kTrendingQueriesModule.name);

  // Control group.
  config.AddGroup(kTrendingQueriesControlGroup, kTrendingQueriesControlID,
                  weight_by_id[kTrendingQueriesControlID]);

  // Experiment Groups
  config.AddGroup(kTrendingQueriesEnabledAllUsersGroup,
                  kTrendingQueriesEnabledAllUsersID,
                  weight_by_id[kTrendingQueriesEnabledAllUsersID]);
  base::FieldTrialParams params;
  params[kTrendingQueriesHideShortcutsParam] = "false";
  params[kTrendingQueriesDisabledFeedParam] = "false";
  params[kTrendingQueriesSignedOutParam] = "false";
  params[kTrendingQueriesNeverShowModuleParam] = "false";
  base::AssociateFieldTrialParams(kTrendingQueriesModule.name,
                                  kTrendingQueriesEnabledAllUsersGroup, params);

  config.AddGroup(kTrendingQueriesEnabledAllUsersHideShortcutsGroup,
                  kTrendingQueriesEnabledAllUsersHideShortcutsID,
                  weight_by_id[kTrendingQueriesEnabledAllUsersHideShortcutsID]);
  params[kTrendingQueriesHideShortcutsParam] = "true";
  params[kTrendingQueriesDisabledFeedParam] = "false";
  params[kTrendingQueriesSignedOutParam] = "false";
  params[kTrendingQueriesNeverShowModuleParam] = "false";
  base::AssociateFieldTrialParams(
      kTrendingQueriesModule.name,
      kTrendingQueriesEnabledAllUsersHideShortcutsGroup, params);

  config.AddGroup(kTrendingQueriesEnabledDisabledFeedGroup,
                  kTrendingQueriesEnabledDisabledFeedID,
                  weight_by_id[kTrendingQueriesEnabledDisabledFeedID]);
  params[kTrendingQueriesHideShortcutsParam] = "false";
  params[kTrendingQueriesDisabledFeedParam] = "true";
  params[kTrendingQueriesSignedOutParam] = "false";
  params[kTrendingQueriesNeverShowModuleParam] = "false";
  base::AssociateFieldTrialParams(kTrendingQueriesModule.name,
                                  kTrendingQueriesEnabledDisabledFeedGroup,
                                  params);

  config.AddGroup(kTrendingQueriesEnabledSignedOutGroup,
                  kTrendingQueriesEnabledSignedOutID,
                  weight_by_id[kTrendingQueriesEnabledSignedOutID]);
  params[kTrendingQueriesHideShortcutsParam] = "true";
  params[kTrendingQueriesDisabledFeedParam] = "false";
  params[kTrendingQueriesSignedOutParam] = "true";
  params[kTrendingQueriesNeverShowModuleParam] = "false";
  base::AssociateFieldTrialParams(kTrendingQueriesModule.name,
                                  kTrendingQueriesEnabledSignedOutGroup,
                                  params);

  config.AddGroup(kTrendingQueriesEnabledNeverShowModuleGroup,
                  kTrendingQueriesEnabledNeverShowModuleID,
                  weight_by_id[kTrendingQueriesEnabledNeverShowModuleID]);
  params[kTrendingQueriesHideShortcutsParam] = "true";
  params[kTrendingQueriesDisabledFeedParam] = "false";
  params[kTrendingQueriesSignedOutParam] = "false";
  params[kTrendingQueriesNeverShowModuleParam] = "true";
  base::AssociateFieldTrialParams(kTrendingQueriesModule.name,
                                  kTrendingQueriesEnabledNeverShowModuleGroup,
                                  params);

  scoped_refptr<base::FieldTrial> trial = config.CreateOneTimeRandomizedTrial(
      kTrendingQueriesDefaultGroup, low_entropy_provider);

  // Finalize the group choice and activates the trial - similar to a variation
  // config that's marked with |starts_active| true. This is required for
  // studies that register variation ids, so they don't reveal extra information
  // beyond the low-entropy source.
  const std::string& group_name = trial->group_name();
  if (group_name == kTrendingQueriesEnabledAllUsersGroup ||
      group_name == kTrendingQueriesEnabledAllUsersHideShortcutsGroup ||
      group_name == kTrendingQueriesEnabledDisabledFeedGroup ||
      group_name == kTrendingQueriesEnabledSignedOutGroup ||
      group_name == kTrendingQueriesEnabledNeverShowModuleGroup) {
    feature_list->RegisterFieldTrialOverride(
        kTrendingQueriesModule.name, base::FeatureList::OVERRIDE_ENABLE_FEATURE,
        trial.get());
  }

  return kCurrentTrialVersion;
}

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kTrialVersionPrefName, kDefaultPrefValue);
}

void Create(const base::FieldTrial::EntropyProvider& low_entropy_provider,
            base::FeatureList* feature_list,
            PrefService* local_state) {
  // The client would not be assigned to any group because features controlled
  // by the experiment is already overridden from the command line. This handles
  // scenarios where FRE is forced for testing purposes.
  // Also return early if the feature is already enabled for this client. This
  // is needed for explicit flag settings in chrome://flags.
  if (feature_list->IsFeatureOverriddenFromCommandLine(
          kTrendingQueriesModule.name) ||
      base::FeatureList::IsEnabled(kTrendingQueriesModule)) {
    return;
  }

  if (local_state->GetInteger(kTrialVersionPrefName) != kDefaultPrefValue) {
    // User has already been bucketed.
    return;
  }

  // Create trial and group for the first time, and store the experiment
  // version in prefs for subsequent runs.
  int trial_version = CreateTrendingQueriesTrial(
      GetGroupWeights(), low_entropy_provider, feature_list);
  // Persist the experiment version for subsequent runs.
  local_state->SetInteger(kTrialVersionPrefName, trial_version);
}

namespace testing {

int CreateTrendingQueriesTrialForTesting(
    std::map<variations::VariationID, int> weights_by_id,
    const base::FieldTrial::EntropyProvider& low_entropy_provider,
    base::FeatureList* feature_list) {
  return CreateTrendingQueriesTrial(weights_by_id, low_entropy_provider,
                                    feature_list);
}

}  // namespace testing

}  // namespace trending_queries_field_trial
