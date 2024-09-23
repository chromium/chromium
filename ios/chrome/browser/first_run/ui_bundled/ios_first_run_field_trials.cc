// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/first_run/ui_bundled/ios_first_run_field_trials.h"

#import "ios/chrome/app/tests_hook.h"

// FirstRunFieldTrialGroup
FirstRunFieldTrialGroup::FirstRunFieldTrialGroup(
    const std::string& name,
    variations::VariationID variation,
    base::FieldTrial::Probability percentage)
    : name_(name), variation_(variation), percentage_(percentage) {}

FirstRunFieldTrialGroup::~FirstRunFieldTrialGroup() {}

// FirstRunFieldTrialConfig
FirstRunFieldTrialConfig::FirstRunFieldTrialConfig(
    const std::string& trial_name)
    : trial_name_(trial_name) {}

FirstRunFieldTrialConfig::~FirstRunFieldTrialConfig() {}

scoped_refptr<base::FieldTrial>
FirstRunFieldTrialConfig::CreateOneTimeRandomizedTrial(
    const std::string& default_group_name,
    const base::FieldTrial::EntropyProvider& low_entropy_provider) {
  DCHECK(!tests_hook::DisableClientSideFieldTrials());
  DCHECK_LE(GetTotalProbability(), 100);
  scoped_refptr<base::FieldTrial> trial =
      base::FieldTrialList::FactoryGetFieldTrial(
          trial_name_, /*total_probability=*/100, default_group_name,
          low_entropy_provider);
  for (const auto& group : groups_) {
    variations::AssociateGoogleVariationID(
        variations::GOOGLE_WEB_PROPERTIES_FIRST_PARTY, trial_name_,
        group.name(), group.variation());
    trial->AppendGroup(group.name(), group.percentage());
  }
  return trial;
}

int FirstRunFieldTrialConfig::GetTotalProbability() {
  int sum = 0;
  for (const auto& group : groups_) {
    sum += group.percentage();
  }
  return sum;
}

void FirstRunFieldTrialConfig::AddGroup(
    const std::string& name,
    variations::VariationID variation,
    base::FieldTrial::Probability percentage) {
  groups_.emplace_back(name, variation, percentage);
}
