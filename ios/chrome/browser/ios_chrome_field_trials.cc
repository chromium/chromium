// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ios_chrome_field_trials.h"

#include "base/path_service.h"
#include "components/metrics/persistent_histograms.h"
#include "ios/chrome/browser/chrome_paths.h"

void IOSChromeFieldTrials::SetupFieldTrials() {
  // Persistent histograms must be enabled as soon as possible.
  base::FilePath user_data_dir;
  if (base::PathService::Get(ios::DIR_USER_DATA, &user_data_dir)) {
    InstantiatePersistentHistograms(user_data_dir);
  }
}

void IOSChromeFieldTrials::SetupFeatureControllingFieldTrials(
    bool has_seed,
    base::FeatureList* feature_list) {
  // Add code here to enable field trials that are active at first run.
  // See http://crrev/c/1128269 for an example.
}
