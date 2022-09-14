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
const int kCurrentTrialVersion = 2;

// Group names for the Trending Queries feature.
const char kTrendingQueriesEnabledModuleEnabledGroup[] =
    "TrendingQueriesEnabledModuleEnabled-V2";
const char kTrendingQueriesEnabledMinimalSpacingModuleEnabledGroup[] =
    "TrendingQueriesEnabledMinimalSpacingModuleEnabled-V2";
const char
    kTrendingQueriesEnabledMinimalSpacingRemoveHeaderModuleEnabledGroup[] =
        "TrendingQueriesEnabledMinimalSpacingRemoveHeaderModuleEnabled-V2";
const char kTrendingQueriesKeepShortcutsEnabledModuleEnabledGroup[] =
    "TrendingQueriesKeepShortcutsEnabledModuleEnabled-V2";
const char kTrendingQueriesControlGroup[] = "Control-V2";

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
      weight_by_id[kTrendingQueriesEnabledModuleEnabledID] = 10;
      weight_by_id[kTrendingQueriesEnabledMinimalSpacingModuleEnabledID] = 10;
      weight_by_id
          [kTrendingQueriesEnabledMinimalSpacingRemoveHeaderModuleEnabledID] =
              10;
      weight_by_id[kTrendingQueriesKeepShortcutsEnabledModuleEnabledID] = 10;
      weight_by_id[kTrendingQueriesControlID] = 10;
      break;
    case version_info::Channel::STABLE:
      break;
  }
  return weight_by_id;
}

// Configures `group_name` with variationID |group_id| of size `group_weight`
// for TrialConfig `config` with the following parameters:
// `start_surface_duration`: double string the new duration of time before
// opening the Start Surface. `should_hide_shortcuts_for_trending_queries`:
// string boolean of whether shortcuts should be hidden for
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
    const std::string& start_surface_duration,
    const std::string& should_hide_shortcuts_for_trending_queries,
    const std::string& should_minimize_spacing_for_modules,
    const std::string& should_remove_headers_for_modules) {
  config.AddGroup(group_name, group_id, group_weight);
  base::FieldTrialParams params;
  params[kReturnToStartSurfaceInactiveDurationInSeconds] =
      start_surface_duration;
  params[kTrendingQueriesHideShortcutsParam] =
      should_hide_shortcuts_for_trending_queries;
  params[kContentSuggestionsUIModuleRefreshMinimizeSpacingParam] =
      should_minimize_spacing_for_modules;
  params[kContentSuggestionsUIModuleRefreshRemoveHeadersParam] =
      should_remove_headers_for_modules;
  base::AssociateFieldTrialParams(kTrendingQueriesFieldTrialName, group_name,
                                  params);
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
  ConfigureGroupForConfig(config, kTrendingQueriesEnabledModuleEnabledGroup,
                          kTrendingQueriesEnabledModuleEnabledID,
                          weight_by_id[kTrendingQueriesEnabledModuleEnabledID],
                          "21600", "true", "false", "false");

  ConfigureGroupForConfig(
      config, kTrendingQueriesEnabledMinimalSpacingModuleEnabledGroup,
      kTrendingQueriesEnabledMinimalSpacingModuleEnabledID,
      weight_by_id[kTrendingQueriesEnabledMinimalSpacingModuleEnabledID],
      "21600", "true", "true", "false");

  ConfigureGroupForConfig(
      config,
      kTrendingQueriesEnabledMinimalSpacingRemoveHeaderModuleEnabledGroup,
      kTrendingQueriesEnabledMinimalSpacingRemoveHeaderModuleEnabledID,
      weight_by_id
          [kTrendingQueriesEnabledMinimalSpacingRemoveHeaderModuleEnabledID],
      "21600", "true", "true", "true");

  ConfigureGroupForConfig(
      config, kTrendingQueriesKeepShortcutsEnabledModuleEnabledGroup,
      kTrendingQueriesKeepShortcutsEnabledModuleEnabledID,
      weight_by_id[kTrendingQueriesKeepShortcutsEnabledModuleEnabledID],
      "21600", "false", "false", "false");

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
        kStartSurface.name, base::FeatureList::OVERRIDE_ENABLE_FEATURE,
        trial.get());
    feature_list->RegisterFieldTrialOverride(
        kTrendingQueriesModule.name, base::FeatureList::OVERRIDE_ENABLE_FEATURE,
        trial.get());
    feature_list->RegisterFieldTrialOverride(
        kContentSuggestionsUIModuleRefresh.name,
        base::FeatureList::OVERRIDE_ENABLE_FEATURE, trial.get());
  } else if (group_name == kTrendingQueriesControlGroup) {
    // Enabled by default, so for consistency's sake also register it in control
    // group.
    feature_list->RegisterFieldTrialOverride(
        kStartSurface.name, base::FeatureList::OVERRIDE_ENABLE_FEATURE,
        trial.get());
    feature_list->RegisterFieldTrialOverride(
        kTrendingQueriesModule.name,
        base::FeatureList::OVERRIDE_DISABLE_FEATURE, trial.get());
    feature_list->RegisterFieldTrialOverride(
        kContentSuggestionsUIModuleRefresh.name,
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
