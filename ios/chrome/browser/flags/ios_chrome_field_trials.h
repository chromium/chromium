// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FLAGS_IOS_CHROME_FIELD_TRIALS_H_
#define IOS_CHROME_BROWSER_FLAGS_IOS_CHROME_FIELD_TRIALS_H_

#include "components/variations/platform_field_trials.h"

// Responsible for setting up field trials specific to iOS. Currently all
// functions are stubs, as iOS has no specific field trials.
class IOSChromeFieldTrials : public variations::PlatformFieldTrials {
 public:
  IOSChromeFieldTrials() {}

  IOSChromeFieldTrials(const IOSChromeFieldTrials&) = delete;
  IOSChromeFieldTrials& operator=(const IOSChromeFieldTrials&) = delete;

  ~IOSChromeFieldTrials() override {}

  // variations::PlatformFieldTrials:
  void OnVariationsSetupComplete() override;
  void SetUpClientSideFieldTrials(
      bool has_seed,
      const variations::EntropyProviders& entropy_providers,
      base::FeatureList* feature_list) override;

 private:
  // Tracks whether a seed has been used to set up field trials.
  bool used_seed_ = false;
};

#endif  // IOS_CHROME_BROWSER_FLAGS_IOS_CHROME_FIELD_TRIALS_H_
