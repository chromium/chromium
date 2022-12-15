// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/ios_popular_sites_field_trial.h"

#import "base/feature_list.h"
#import "base/metrics/field_trial.h"
#import "base/metrics/field_trial_params.h"
#import "components/ntp_tiles/features.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/first_run/first_run.h"
#import "ios/chrome/browser/ui/first_run/ios_first_run_field_trials.h"
#import "ios/chrome/browser/ui/ntp/field_trial_constants.h"
#import "ios/chrome/common/channel_info.h"

namespace {

// The placeholder trial version that is stored for a client who has not been
// enrolled in the experiment.
const int kPlaceholderTrialVersion = -1;

// Store local state preference with whether the client has participated in
// kIOSPopularSitesImprovedSuggestionsFieldTrialName experiment or not.
const char kTrialPrefName[] = "popular_sites.trial_version";

// The current trial version of
// kIOSPopularSitesImprovedSuggestionsFieldTrialName; should be updated when
// the experiment is modified.
const int kCurrentTrialVersion = 1;

// Returns a map of the group weights for each arm.
std::map<variations::VariationID, int> GetGroupWeights() {
  std::map<variations::VariationID, int> weight_by_id = {
      {field_trial_constants::
           kIOSPopularSitesImprovedSuggestionsWithAppsEnabledID,
       0},
      {field_trial_constants::
           kIOSPopularSitesImprovedSuggestionsWithoutAppsEnabledID,
       0},
      {field_trial_constants::kIOSPopularSitesImprovedSuggestionsControlID, 0}};

  switch (GetChannel()) {
    case version_info::Channel::UNKNOWN:
      [[fallthrough]];
    case version_info::Channel::CANARY:
      [[fallthrough]];
    case version_info::Channel::DEV:
      [[fallthrough]];
    case version_info::Channel::BETA:
      weight_by_id[field_trial_constants::
                       kIOSPopularSitesImprovedSuggestionsWithAppsEnabledID] =
          25;
      weight_by_id
          [field_trial_constants::
               kIOSPopularSitesImprovedSuggestionsWithoutAppsEnabledID] = 25;
      weight_by_id[field_trial_constants::
                       kIOSPopularSitesImprovedSuggestionsControlID] = 25;
      break;
    case version_info::Channel::STABLE:
      break;
  }

  return weight_by_id;
}

}  // namespace

namespace ios_popular_sites_field_trial {

// Creates the trial config, initializes the trial that puts clients into
// different groups, and returns the version number of the current trial. There
// are 3 groups other than the default group:
// - Control
// - Enabled (with Big Apps)
// - Enabled (without Big Apps)
void CreateImprovedSuggestionsTrial(
    std::map<variations::VariationID, int> weight_by_id,
    const base::FieldTrial::EntropyProvider& low_entropy_provider,
    base::FeatureList* feature_list) {
  FirstRunFieldTrialConfig config(
      field_trial_constants::kIOSPopularSitesImprovedSuggestionsFieldTrialName);

  config.AddGroup(
      field_trial_constants::kIOSPopularSitesImprovedSuggestionsControlGroup,
      field_trial_constants::kIOSPopularSitesImprovedSuggestionsControlID,
      weight_by_id[field_trial_constants::
                       kIOSPopularSitesImprovedSuggestionsControlID]);

  config.AddGroup(
      field_trial_constants::
          kIOSPopularSitesImprovedSuggestionsWithAppsEnabledGroup,
      field_trial_constants::
          kIOSPopularSitesImprovedSuggestionsWithAppsEnabledID,
      weight_by_id[field_trial_constants::
                       kIOSPopularSitesImprovedSuggestionsWithAppsEnabledID]);

  // Explicitly set `kIOSPopularSitesExcludePopularAppsParam` to false and
  // associate it with the
  // `kIOSPopularSitesImprovedSuggestionsWithAppsEnabledGroup`
  base::FieldTrialParams with_apps_enabled_params;
  with_apps_enabled_params[ntp_tiles::kIOSPopularSitesExcludePopularAppsParam] =
      "false";
  base::AssociateFieldTrialParams(
      field_trial_constants::kIOSPopularSitesImprovedSuggestionsFieldTrialName,
      field_trial_constants::
          kIOSPopularSitesImprovedSuggestionsWithAppsEnabledGroup,
      with_apps_enabled_params);

  config.AddGroup(
      field_trial_constants::
          kIOSPopularSitesImprovedSuggestionsWithoutAppsEnabledGroup,
      field_trial_constants::
          kIOSPopularSitesImprovedSuggestionsWithoutAppsEnabledID,
      weight_by_id
          [field_trial_constants::
               kIOSPopularSitesImprovedSuggestionsWithoutAppsEnabledID]);

  // Explicitly set `kIOSPopularSitesExcludePopularAppsParam` to true and
  // associate it with the
  // `kIOSPopularSitesImprovedSuggestionsWithAppsEnabledGroup`
  base::FieldTrialParams without_apps_enabled_params;
  without_apps_enabled_params
      [ntp_tiles::kIOSPopularSitesExcludePopularAppsParam] = "true";
  base::AssociateFieldTrialParams(
      field_trial_constants::kIOSPopularSitesImprovedSuggestionsFieldTrialName,
      field_trial_constants::
          kIOSPopularSitesImprovedSuggestionsWithoutAppsEnabledGroup,
      without_apps_enabled_params);

  scoped_refptr<base::FieldTrial> trial = config.CreateOneTimeRandomizedTrial(
      field_trial_constants::kIOSPopularSitesDefaultSuggestionsGroup,
      low_entropy_provider);

  // Finalize the group choice and activates the trial - similar to a variation
  // config that's marked with `starts_active` true. This is required for
  // studies that register variation ids, so they don't reveal extra information
  // beyond the low-entropy source.
  const std::string& group_name = trial->group_name();

  if (group_name ==
          field_trial_constants::
              kIOSPopularSitesImprovedSuggestionsWithAppsEnabledGroup ||
      group_name ==
          field_trial_constants::
              kIOSPopularSitesImprovedSuggestionsWithoutAppsEnabledGroup) {
    feature_list->RegisterFieldTrialOverride(
        ntp_tiles::kIOSPopularSitesImprovedSuggestions.name,
        base::FeatureList::OVERRIDE_ENABLE_FEATURE, trial.get());
  } else if (group_name ==
             field_trial_constants::
                 kIOSPopularSitesImprovedSuggestionsControlGroup) {
    feature_list->RegisterFieldTrialOverride(
        ntp_tiles::kIOSPopularSitesImprovedSuggestions.name,
        base::FeatureList::OVERRIDE_DISABLE_FEATURE, trial.get());
  }
}

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kTrialPrefName, kPlaceholderTrialVersion);
}

void Create(const base::FieldTrial::EntropyProvider& low_entropy_provider,
            base::FeatureList* feature_list,
            PrefService* local_state) {
  // Don't create the trial if the feature is overridden to avoid having
  // multiple registered trials for the same feature.
  if (feature_list->IsFeatureOverridden(
          field_trial_constants::
              kIOSPopularSitesImprovedSuggestionsFieldTrialName)) {
    return;
  }

  // If the client is already an existing client by the time this experiment
  // began running, don't register (e.g. the client is not in a First Run
  // experience and was never grouped client-side into this study when it went
  // through First Run).

  // If the user is enrolled in a previous version of the same experiment,
  // exclude them out of the current version.
  if (!FirstRun::IsChromeFirstRun() &&
      local_state->GetInteger(kTrialPrefName) != kCurrentTrialVersion) {
    return;
  }

  // Enroll first run clients in the experiment.
  // If the client is enrolled in the current version of the experiment,
  // register the trial to keep them in the experiment; they will be placed
  // in the same group because `low_entropy_provider` is persisted across
  // launches.
  CreateImprovedSuggestionsTrial(GetGroupWeights(), low_entropy_provider,
                                 feature_list);

  local_state->SetInteger(kTrialPrefName, kCurrentTrialVersion);
}

void CreateImprovedSuggestionsTrialForTesting(
    std::map<variations::VariationID, int> weights_by_id,
    const base::FieldTrial::EntropyProvider& low_entropy_provider,
    base::FeatureList* feature_list) {
  CreateImprovedSuggestionsTrial(weights_by_id, low_entropy_provider,
                                 feature_list);
}

}  // namespace ios_popular_sites_field_trial
