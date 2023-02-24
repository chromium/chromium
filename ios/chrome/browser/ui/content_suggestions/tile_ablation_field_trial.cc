// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/content_suggestions/tile_ablation_field_trial.h"

#import "base/feature_list.h"
#import "base/metrics/field_trial.h"
#import "base/metrics/field_trial_params.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/first_run/first_run.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/content_suggestions/field_trial_constants.h"
#import "ios/chrome/browser/ui/first_run/ios_first_run_field_trials.h"
#import "ios/chrome/common/channel_info.h"

namespace tile_ablation_field_trial {
// The placeholder trial version that is stored for a client who has not been
// enrolled in the experiment.
const int kPlaceholderTrialVersion = -1;

// Store local state preference with whether the client has participated in
// kTileAblationMVTAndShortcutsFieldTrialName experiment or not.
const char kTrialPrefName[] = "hide_mvt_shortcuts.trial_version";

// The current trial version of
// kTileAblationMVTAndShortcutsFieldTrialName; should be updated when
// the experiment is modified.
const int kCurrentTrialVersion = 1;

// Returns a map of the group weights for each arm.
std::map<variations::VariationID, int> GetGroupWeights() {
  std::map<variations::VariationID, int> weight_by_id = {
      {field_trial_constants::kTileAblationMVTOnlyID, 0},
      {field_trial_constants::kTileAblationMVTAndShortcutsID, 0},
      {field_trial_constants::kTileAblationControlID, 0}};

  return weight_by_id;
}

// Creates the trial config, initializes the trial that puts clients into
// different groups, and returns the version number of the current trial. There
// are 3 groups other than the default group:
// - Control (Normal NTP)
// - Hidden (only MVTs)
// - Hidden (MVTs and Shortcuts)
void CreateTileAblationTrial(
    std::map<variations::VariationID, int> weight_by_id,
    const base::FieldTrial::EntropyProvider& low_entropy_provider,
    base::FeatureList* feature_list) {
  FirstRunFieldTrialConfig config(
      field_trial_constants::kTileAblationFieldTrialName);

  config.AddGroup(
      field_trial_constants::kTileAblationMVTAndShortcutsControlGroup,
      field_trial_constants::kTileAblationControlID,
      weight_by_id[field_trial_constants::kTileAblationControlID]);

  config.AddGroup(field_trial_constants::kTileAblationMVTOnlyGroup,
                  field_trial_constants::kTileAblationMVTOnlyID,
                  weight_by_id[field_trial_constants::kTileAblationMVTOnlyID]);

  // Explicitly set `kTileAblationMVTOnlyParam` to true and associate it with
  // the `kTileAblationMVTOnlyGroup`
  base::FieldTrialParams only_mvt_params;
  only_mvt_params[kTileAblationMVTOnlyParam] = "true";
  base::AssociateFieldTrialParams(
      field_trial_constants::kTileAblationFieldTrialName,
      field_trial_constants::kTileAblationMVTOnlyGroup, only_mvt_params);

  config.AddGroup(
      field_trial_constants::kTileAblationMVTAndShortcutsGroup,
      field_trial_constants::kTileAblationMVTAndShortcutsID,
      weight_by_id[field_trial_constants::kTileAblationMVTAndShortcutsID]);

  // Explicitly set `kTileAblationMVTOnlyParam` to false and associate it with
  // the `kTileAblationMVTAndShortcutsGroup`
  base::FieldTrialParams mvt_and_shortcuts_params;
  mvt_and_shortcuts_params[kTileAblationMVTOnlyParam] = "false";
  base::AssociateFieldTrialParams(
      field_trial_constants::kTileAblationFieldTrialName,
      field_trial_constants::kTileAblationMVTAndShortcutsGroup,
      mvt_and_shortcuts_params);

  scoped_refptr<base::FieldTrial> trial = config.CreateOneTimeRandomizedTrial(
      field_trial_constants::kTileAblationMVTAndShortcutsDefaultGroup,
      low_entropy_provider);

  // Finalize the group choice and activates the trial - similar to a variation
  // config that's marked with `starts_active` true. This is required for
  // studies that register variation ids, so they don't reveal extra information
  // beyond the low-entropy source.
  const std::string& group_name = trial->group_name();

  if (group_name == field_trial_constants::kTileAblationMVTOnlyGroup ||
      group_name == field_trial_constants::kTileAblationMVTAndShortcutsGroup) {
    feature_list->RegisterFieldTrialOverride(
        kTileAblation.name, base::FeatureList::OVERRIDE_ENABLE_FEATURE,
        trial.get());
  } else if (group_name ==
             field_trial_constants::kTileAblationMVTAndShortcutsControlGroup) {
    feature_list->RegisterFieldTrialOverride(
        kTileAblation.name, base::FeatureList::OVERRIDE_DISABLE_FEATURE,
        trial.get());
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
          field_trial_constants::kTileAblationFieldTrialName)) {
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
  CreateTileAblationTrial(GetGroupWeights(), low_entropy_provider,
                          feature_list);

  local_state->SetInteger(kTrialPrefName, kCurrentTrialVersion);
}

void CreateTileAblationTrialForTesting(
    std::map<variations::VariationID, int> weights_by_id,
    const base::FieldTrial::EntropyProvider& low_entropy_provider,
    base::FeatureList* feature_list) {
  CreateTileAblationTrial(weights_by_id, low_entropy_provider, feature_list);
}

}  // namespace tile_ablation_field_trial
