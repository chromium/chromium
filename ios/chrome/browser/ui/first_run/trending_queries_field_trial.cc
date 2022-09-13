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

// Name of the Trending Queries Field Trial.
const char kTrendingQueriesFieldTrialName[] = "TrendingQueriesNewUsers";

namespace {
// Store local state preference with whether the client has participated in
// kTrendingQueriesFieldTrialName experiment or not.
const char kTrialPrefName[] = "trending_queries.trial_version";
// The placeholder trial version that is stored for a client who has not been
// enrolled in the experiment.
const int kPlaceholderTrialVersion = -1;
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
// are 6 groups other than the default group:
// - Control
// - Enabled for All users
// - Enabled for All users and Hide Shortcuts
// - Enabled for signed out users
// - Enabled for users that had the feed disabled
// - Disabled for All users and Hide Shortcuts (essentially only showing
// Most Visited and pushing up the feed)
void CreateTrendingQueriesTrial(
    std::map<variations::VariationID, int> weight_by_id,
    const base::FieldTrial::EntropyProvider& low_entropy_provider,
    base::FeatureList* feature_list) {
  FirstRunFieldTrialConfig config(kTrendingQueriesFieldTrialName);

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
  base::AssociateFieldTrialParams(kTrendingQueriesFieldTrialName,
                                  kTrendingQueriesEnabledAllUsersGroup, params);

  config.AddGroup(kTrendingQueriesEnabledAllUsersHideShortcutsGroup,
                  kTrendingQueriesEnabledAllUsersHideShortcutsID,
                  weight_by_id[kTrendingQueriesEnabledAllUsersHideShortcutsID]);
  params[kTrendingQueriesHideShortcutsParam] = "true";
  params[kTrendingQueriesDisabledFeedParam] = "false";
  params[kTrendingQueriesSignedOutParam] = "false";
  params[kTrendingQueriesNeverShowModuleParam] = "false";
  base::AssociateFieldTrialParams(
      kTrendingQueriesFieldTrialName,
      kTrendingQueriesEnabledAllUsersHideShortcutsGroup, params);

  config.AddGroup(kTrendingQueriesEnabledDisabledFeedGroup,
                  kTrendingQueriesEnabledDisabledFeedID,
                  weight_by_id[kTrendingQueriesEnabledDisabledFeedID]);
  params[kTrendingQueriesHideShortcutsParam] = "false";
  params[kTrendingQueriesDisabledFeedParam] = "true";
  params[kTrendingQueriesSignedOutParam] = "false";
  params[kTrendingQueriesNeverShowModuleParam] = "false";
  base::AssociateFieldTrialParams(kTrendingQueriesFieldTrialName,
                                  kTrendingQueriesEnabledDisabledFeedGroup,
                                  params);

  config.AddGroup(kTrendingQueriesEnabledSignedOutGroup,
                  kTrendingQueriesEnabledSignedOutID,
                  weight_by_id[kTrendingQueriesEnabledSignedOutID]);
  params[kTrendingQueriesHideShortcutsParam] = "true";
  params[kTrendingQueriesDisabledFeedParam] = "false";
  params[kTrendingQueriesSignedOutParam] = "true";
  params[kTrendingQueriesNeverShowModuleParam] = "false";
  base::AssociateFieldTrialParams(kTrendingQueriesFieldTrialName,
                                  kTrendingQueriesEnabledSignedOutGroup,
                                  params);

  config.AddGroup(kTrendingQueriesEnabledNeverShowModuleGroup,
                  kTrendingQueriesEnabledNeverShowModuleID,
                  weight_by_id[kTrendingQueriesEnabledNeverShowModuleID]);
  params[kTrendingQueriesHideShortcutsParam] = "true";
  params[kTrendingQueriesDisabledFeedParam] = "false";
  params[kTrendingQueriesSignedOutParam] = "false";
  params[kTrendingQueriesNeverShowModuleParam] = "true";
  base::AssociateFieldTrialParams(kTrendingQueriesFieldTrialName,
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
  } else if (group_name == kTrendingQueriesControlGroup) {
    feature_list->RegisterFieldTrialOverride(
        kTrendingQueriesModule.name,
        base::FeatureList::OVERRIDE_DISABLE_FEATURE, trial.get());
  }
}

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kTrialPrefName, kPlaceholderTrialVersion);
}

void Create(const base::FieldTrial::EntropyProvider& low_entropy_provider,
            base::FeatureList* feature_list,
            PrefService* local_state) {
  // Don't create the trial if it was already created for testing. This is only
  // expected when the browser is used for development purpose. The trial
  // created when the about flag is set will have the same name as the
  // `kTrendingQueriesModule`. This condition is to avoid having multiple trials
  // overriding the same feature. A trial might have also been created with the
  // commandline arguments.
  if (base::FieldTrialList::TrialExists(kTrendingQueriesFieldTrialName)) {
    return;
  }

  // If the client is already an existing client by the time this experiment
  // began running, don't register (e.g. the client is not in a First Run
  // experience and was never grouped client-side into this study when it went 
  // through First Run).
  if (!FirstRun::IsChromeFirstRun() &&
      local_state->GetInteger(kTrialPrefName) != kCurrentTrialVersion) {
    return;
  }

  CreateTrendingQueriesTrial(GetGroupWeights(), low_entropy_provider,
                             feature_list);
  local_state->SetInteger(kTrialPrefName, kCurrentTrialVersion);
}

void CreateTrendingQueriesTrialForTesting(
    std::map<variations::VariationID, int> weights_by_id,
    const base::FieldTrial::EntropyProvider& low_entropy_provider,
    base::FeatureList* feature_list) {
  CreateTrendingQueriesTrial(weights_by_id, low_entropy_provider, feature_list);
}

}  // namespace trending_queries_field_trial
