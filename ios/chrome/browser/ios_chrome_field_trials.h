// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_IOS_CHROME_FIELD_TRIALS_H_
#define IOS_CHROME_BROWSER_IOS_CHROME_FIELD_TRIALS_H_

#include "base/macros.h"
#include "components/variations/platform_field_trials.h"

// Responsible for setting up field trials specific to iOS. Currently all
// functions are stubs, as iOS has no specific field trials.
class IOSChromeFieldTrials : public variations::PlatformFieldTrials {
 public:
  IOSChromeFieldTrials() {}
  ~IOSChromeFieldTrials() override {}

  // variations::PlatformFieldTrials:
  void SetupFieldTrials() override;
  void SetupFeatureControllingFieldTrials(
      bool has_seed,
      base::FeatureList* feature_list) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(IOSChromeFieldTrials);
};

#endif  // IOS_CHROME_BROWSER_IOS_CHROME_FIELD_TRIALS_H_
