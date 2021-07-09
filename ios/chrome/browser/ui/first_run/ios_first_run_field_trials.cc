// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/first_run/ios_first_run_field_trials.h"

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
  scoped_refptr<base::FieldTrial> trial =
      base::FieldTrialList::FactoryGetFieldTrialWithRandomizationSeed(
          trial_name_, GetTotalProbability(), default_group_name,
          base::FieldTrial::ONE_TIME_RANDOMIZED, 0,
          /*default_group_number=*/nullptr, &low_entropy_provider);
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
