// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_IOS_FIRST_RUN_FIELD_TRIALS_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_IOS_FIRST_RUN_FIELD_TRIALS_H_

#include <string>
#include <vector>

#include "base/metrics/field_trial.h"
#include "components/variations/variations_associated_data.h"

// Helper class defining a FieldTrial group.
class FirstRunFieldTrialGroup {
 public:
  FirstRunFieldTrialGroup(const std::string& name,
                          variations::VariationID variation,
                          base::FieldTrial::Probability percentage);
  ~FirstRunFieldTrialGroup();

  // Accessors for this FieldTrial group.
  const std::string& name() const { return name_; }
  variations::VariationID variation() const { return variation_; }
  base::FieldTrial::Probability percentage() const { return percentage_; }

 private:
  std::string name_;
  variations::VariationID variation_;
  base::FieldTrial::Probability percentage_;
};

// Helper class defining a FieldTrial configuration that will be valid at the
// time of Chrome First Run. Since server-provided FieldTrial configurations
// that need to be downloaded will not take effect until at least the second
// run, FieldTrials involving substantial UI changes that need to take effect
// at first run must be pre-defined in client code.
class FirstRunFieldTrialConfig {
 public:
  // Initializes with `trial_name` as the name of the FieldTrial.
  FirstRunFieldTrialConfig(const std::string& trial_name);

  FirstRunFieldTrialConfig(const FirstRunFieldTrialConfig&) = delete;
  FirstRunFieldTrialConfig& operator=(const FirstRunFieldTrialConfig&) = delete;

  ~FirstRunFieldTrialConfig();

  // Creates and returns a one-time randomized FieldTrial with
  // `default_group_name` with groups configured with Google Variation IDs.
  scoped_refptr<base::FieldTrial> CreateOneTimeRandomizedTrial(
      const std::string& default_group_name,
      const base::FieldTrial::EntropyProvider& low_entropy_provider);

  // Adds a new FieldTrial group of `name` with a probability of `percentage`.
  // `variation` defines a server-side variation configuration (non-trigger).
  void AddGroup(const std::string& name,
                variations::VariationID variation,
                base::FieldTrial::Probability percentage);

  // Gets the probability sum of every group in the trial.
  int GetTotalProbability();

  // Returns a vector of FieldTrial groups for this FieldTrial configuration.
  const std::vector<FirstRunFieldTrialGroup>& groups() const { return groups_; }
  // Accessors for this FieldTrial.
  const std::string& trial_name() { return trial_name_; }

 private:
  std::string trial_name_;
  std::vector<FirstRunFieldTrialGroup> groups_;
};

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_IOS_FIRST_RUN_FIELD_TRIALS_H_
