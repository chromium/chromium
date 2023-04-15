// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/ntp/synced_segments_field_trial.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "components/history/core/browser/features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "ios/chrome/browser/first_run/first_run.h"
#include "ios/chrome/browser/ui/first_run/ios_first_run_field_trials.h"
#include "ios/chrome/browser/ui/ntp/synced_segments_field_trial_constants.h"
#include "ios/chrome/common/channel_info.h"

namespace {

// The placeholder trial version that is stored for a client who has not been
// enrolled in the experiment.
const int kPlaceholderTrialVersion = -1;

// Store local state preference with whether the client has participated in
// `kIOSSyncedSegmentsFieldTrialName` experiment or not.
const char kTrialPrefName[] = "synced_segments.trial_version";

// The current trial version of
// `kIOSSyncedSegmentsFieldTrialName`; should be updated when
// the experiment is modified.
const int kCurrentTrialVersion = 1;

// Returns a map of the group weights for each arm.
std::map<variations::VariationID, int> GetGroupWeights() {
  std::map<variations::VariationID, int> weight_by_id = {
      {synced_segments_field_trial_constants::kIOSSyncedSegmentsEnabledID, 0},
      {synced_segments_field_trial_constants::kIOSSyncedSegmentsControlID, 0}};

  return weight_by_id;
}

}  // namespace

namespace synced_segments_field_trial {

// Creates the trial config, initializes the trial that puts clients into
// different groups, and returns the version number of the current trial.
void CreateSyncedSegmentsTrial(
    std::map<variations::VariationID, int> weight_by_id,
    const base::FieldTrial::EntropyProvider& low_entropy_provider,
    base::FeatureList* feature_list) {
  FirstRunFieldTrialConfig config(
      synced_segments_field_trial_constants::kIOSSyncedSegmentsFieldTrialName);

  config.AddGroup(
      synced_segments_field_trial_constants::kIOSSyncedSegmentsControlGroup,
      synced_segments_field_trial_constants::kIOSSyncedSegmentsControlID,
      weight_by_id
          [synced_segments_field_trial_constants::kIOSSyncedSegmentsControlID]);

  config.AddGroup(
      synced_segments_field_trial_constants::kIOSSyncedSegmentsEnabledGroup,
      synced_segments_field_trial_constants::kIOSSyncedSegmentsEnabledID,
      weight_by_id
          [synced_segments_field_trial_constants::kIOSSyncedSegmentsEnabledID]);

  scoped_refptr<base::FieldTrial> trial = config.CreateOneTimeRandomizedTrial(
      synced_segments_field_trial_constants::kIOSSyncedSegmentsControlGroup,
      low_entropy_provider);

  // Finalize the group choice and activates the trial - similar to a variation
  // config that's marked with `starts_active` true. This is required for
  // studies that register variation ids, so they don't reveal extra information
  // beyond the low-entropy source.
  const std::string& group_name = trial->group_name();

  if (group_name ==
      synced_segments_field_trial_constants::kIOSSyncedSegmentsEnabledGroup) {
    feature_list->RegisterFieldTrialOverride(
        history::kSyncSegmentsData.name,
        base::FeatureList::OVERRIDE_ENABLE_FEATURE, trial.get());
  } else if (group_name == synced_segments_field_trial_constants::
                               kIOSSyncedSegmentsControlGroup) {
    feature_list->RegisterFieldTrialOverride(
        history::kSyncSegmentsData.name,
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
  if (feature_list->IsFeatureOverridden(synced_segments_field_trial_constants::
                                            kIOSSyncedSegmentsFieldTrialName)) {
    return;
  }

  // If the client is already an existing client by the time this experiment
  // began running, don't register (e.g. the client is not in a First Run
  // experience and was never grouped client-side into this study when it went
  // through First Run). If the user is enrolled in a previous version of the
  // same experiment, exclude them out of the current version.
  if (!FirstRun::IsChromeFirstRun() &&
      local_state->GetInteger(kTrialPrefName) != kCurrentTrialVersion) {
    return;
  }

  // Enroll first run clients in the experiment.
  // If the client is enrolled in the current version of the experiment,
  // register the trial to keep them in the experiment; they will be placed
  // in the same group because `low_entropy_provider` is persisted across
  // launches.
  CreateSyncedSegmentsTrial(GetGroupWeights(), low_entropy_provider,
                            feature_list);

  local_state->SetInteger(kTrialPrefName, kCurrentTrialVersion);
}

void CreateSyncedSegmentsTrialForTesting(
    std::map<variations::VariationID, int> weights_by_id,
    const base::FieldTrial::EntropyProvider& low_entropy_provider,
    base::FeatureList* feature_list) {
  CreateSyncedSegmentsTrial(weights_by_id, low_entropy_provider, feature_list);
}

}  // namespace synced_segments_field_trial
