// Copyright 2022 The Chromium Authors
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
#import "ios/chrome/browser/ui/start_surface/start_surface_features.h"
#import "ios/chrome/common/channel_info.h"

namespace {
// The placeholder trial version that is stored for a client who has not been
// enrolled in the experiment.
const int kPlaceholderTrialVersion = -1;

// Group names for the Trending Queries feature.
const char kTrendingQueriesEnabledModuleEnabledGroup[] =
    "TrendingQueriesEnabledModuleEnabled-V3";
const char kTrendingQueriesEnabledMinimalSpacingModuleEnabledGroup[] =
    "TrendingQueriesEnabledMinimalSpacingModuleEnabled-V3";
const char
    kTrendingQueriesEnabledMinimalSpacingRemoveHeaderModuleEnabledGroup[] =
        "TrendingQueriesEnabledMinimalSpacingRemoveHeaderModuleEnabled-V3";
const char kTrendingQueriesKeepShortcutsEnabledModuleEnabledGroup[] =
    "TrendingQueriesKeepShortcutsEnabledModuleEnabled-V3";
const char kTrendingQueriesControlGroup[] = "Control-V3";

const char kTrendingQueriesDefaultGroup[] = "Default";

// Returns a map of the group weights for each arm.
std::map<variations::VariationID, int> GetGroupWeights() {
  std::map<variations::VariationID, int> weight_by_id = {
      {kTrendingQueriesEnabledModuleEnabledID, 0},
      {kTrendingQueriesEnabledMinimalSpacingModuleEnabledID, 0},
      {kTrendingQueriesEnabledMinimalSpacingRemoveHeaderModuleEnabledID, 0},
      {kTrendingQueriesKeepShortcutsEnabledModuleEnabledID, 0},
      {kTrendingQueriesControlID, 0}};
  switch (GetChannel()) {
    case version_info::Channel::UNKNOWN:
    case version_info::Channel::CANARY:
    case version_info::Channel::DEV:
    case version_info::Channel::BETA:
      weight_by_id[kTrendingQueriesEnabledModuleEnabledID] = 20;
      weight_by_id[kTrendingQueriesEnabledMinimalSpacingModuleEnabledID] = 20;
      weight_by_id
          [kTrendingQueriesEnabledMinimalSpacingRemoveHeaderModuleEnabledID] =
              20;
      weight_by_id[kTrendingQueriesKeepShortcutsEnabledModuleEnabledID] = 20;
      weight_by_id[kTrendingQueriesControlID] = 20;
      break;
    case version_info::Channel::STABLE:
      weight_by_id[kTrendingQueriesEnabledModuleEnabledID] = 8;
      weight_by_id[kTrendingQueriesEnabledMinimalSpacingModuleEnabledID] = 8;
      weight_by_id
          [kTrendingQueriesEnabledMinimalSpacingRemoveHeaderModuleEnabledID] =
              8;
      weight_by_id[kTrendingQueriesKeepShortcutsEnabledModuleEnabledID] = 8;
      weight_by_id[kTrendingQueriesControlID] = 8;
      break;
  }
  return weight_by_id;
}

// Configures `group_name` with variationID |group_id| of size `group_weight`
// for TrialConfig `config` with the following parameters:
// kTrendingQueriesModule. `should_minimize_spacing_for_modules`: string boolean
// of whether to minimize spacing in kContentSuggestionsUIModuleRefresh.
// `should_remove_headers_for_modules`: string boolean of whether the header
// should not be shown in kContentSuggestionsUIModuleRefresh. See
// content_suggestions_feature.h for more details about params.
void ConfigureGroupForConfig(
    FirstRunFieldTrialConfig& config,
    const std::string& group_name,
    const variations::VariationID group_id,
    int group_weight,
    const std::string& should_hide_shortcuts_for_trending_queries,
    const std::string& should_minimize_spacing_for_modules,
    const std::string& should_remove_headers_for_modules) {
  config.AddGroup(group_name, group_id, group_weight);
  base::FieldTrialParams params;
  params[kTrendingQueriesHideShortcutsParam] =
      should_hide_shortcuts_for_trending_queries;
  params[kContentSuggestionsUIModuleRefreshMinimizeSpacingParam] =
      should_minimize_spacing_for_modules;
  params[kContentSuggestionsUIModuleRefreshRemoveHeadersParam] =
      should_remove_headers_for_modules;
  base::AssociateFieldTrialParams(
      kModularHomeTrendingQueriesClientSideFieldTrialName, group_name, params);
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
  FirstRunFieldTrialConfig config(
      kModularHomeTrendingQueriesClientSideFieldTrialName);

  // Control group.
  config.AddGroup(kTrendingQueriesControlGroup, kTrendingQueriesControlID,
                  weight_by_id[kTrendingQueriesControlID]);

  // Experiment Groups
  ConfigureGroupForConfig(config, kTrendingQueriesEnabledModuleEnabledGroup,
                          kTrendingQueriesEnabledModuleEnabledID,
                          weight_by_id[kTrendingQueriesEnabledModuleEnabledID],
                          "true", "false", "false");

  ConfigureGroupForConfig(
      config, kTrendingQueriesEnabledMinimalSpacingModuleEnabledGroup,
      kTrendingQueriesEnabledMinimalSpacingModuleEnabledID,
      weight_by_id[kTrendingQueriesEnabledMinimalSpacingModuleEnabledID],
      "true", "true", "false");

  ConfigureGroupForConfig(
      config,
      kTrendingQueriesEnabledMinimalSpacingRemoveHeaderModuleEnabledGroup,
      kTrendingQueriesEnabledMinimalSpacingRemoveHeaderModuleEnabledID,
      weight_by_id
          [kTrendingQueriesEnabledMinimalSpacingRemoveHeaderModuleEnabledID],
      "true", "true", "true");

  ConfigureGroupForConfig(
      config, kTrendingQueriesKeepShortcutsEnabledModuleEnabledGroup,
      kTrendingQueriesKeepShortcutsEnabledModuleEnabledID,
      weight_by_id[kTrendingQueriesKeepShortcutsEnabledModuleEnabledID],
      "false", "false", "false");

  scoped_refptr<base::FieldTrial> trial = config.CreateOneTimeRandomizedTrial(
      kTrendingQueriesDefaultGroup, low_entropy_provider);

  // Finalize the group choice and activates the trial - similar to a variation
  // config that's marked with `starts_active` true. This is required for
  // studies that register variation ids, so they don't reveal extra information
  // beyond the low-entropy source.
  const std::string& group_name = trial->group_name();
  if (group_name == kTrendingQueriesEnabledModuleEnabledGroup ||
      group_name == kTrendingQueriesEnabledMinimalSpacingModuleEnabledGroup ||
      group_name ==
          kTrendingQueriesEnabledMinimalSpacingRemoveHeaderModuleEnabledGroup ||
      group_name == kTrendingQueriesKeepShortcutsEnabledModuleEnabledGroup) {
    feature_list->RegisterFieldTrialOverride(
        kTrendingQueriesModuleNewUser.name,
        base::FeatureList::OVERRIDE_ENABLE_FEATURE, trial.get());
    feature_list->RegisterFieldTrialOverride(
        kContentSuggestionsUIModuleRefreshNewUser.name,
        base::FeatureList::OVERRIDE_ENABLE_FEATURE, trial.get());
  } else if (group_name == kTrendingQueriesControlGroup) {
    feature_list->RegisterFieldTrialOverride(
        kTrendingQueriesModuleNewUser.name,
        base::FeatureList::OVERRIDE_DISABLE_FEATURE, trial.get());
    feature_list->RegisterFieldTrialOverride(
        kContentSuggestionsUIModuleRefreshNewUser.name,
        base::FeatureList::OVERRIDE_DISABLE_FEATURE, trial.get());
  }
}

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kTrialPrefName, kPlaceholderTrialVersion);
}

void Create(const base::FieldTrial::EntropyProvider& low_entropy_provider,
            base::FeatureList* feature_list,
            PrefService* local_state) {
  // Don't create the trial if either feature is enabled in chrome://flags. This
  // condition is to avoid having multiple registered trials overriding the same
  // feature.
  if (feature_list->IsFeatureOverridden(
          kContentSuggestionsUIModuleRefreshNewUser.name) ||
      feature_list->IsFeatureOverridden(kTrendingQueriesModuleNewUser.name)) {
    return;
  }

  // If the client is already an existing client by the time this experiment
  // began running, don't register (e.g. the client is not in a First Run
  // experience and was never grouped client-side into this study when it went
  // through First Run).
  // If this is not First Run, but the client has the correct pref saved, that
  // means the user was bucketed into the trial when it went through First Run.
  // Thus, it is important to register the trial, so those clients can persist
  // the behavior that was chosen on first run.
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
