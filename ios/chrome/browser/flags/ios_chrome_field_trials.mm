// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/flags/ios_chrome_field_trials.h"

#import "base/check.h"
#import "base/path_service.h"
#import "components/metrics/persistent_histograms.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/shared/model/paths/paths.h"
#import "ios/chrome/browser/variations/model/ios_chrome_variations_seed_store.h"

void IOSChromeFieldTrials::OnVariationsSetupComplete() {
  // Persistent histograms must be enabled ASAP, but depends on Features.
  base::FilePath user_data_dir;
  if (base::PathService::Get(ios::DIR_USER_DATA, &user_data_dir)) {
    InstantiatePersistentHistogramsWithFeaturesAndCleanup(user_data_dir);
  } else {
    NOTREACHED_IN_MIGRATION();
  }
  if (used_seed_) {
    [IOSChromeVariationsSeedStore notifySeedApplication];
  }
}

void IOSChromeFieldTrials::SetUpClientSideFieldTrials(
    bool has_seed,
    const variations::EntropyProviders& entropy_providers,
    base::FeatureList* feature_list) {
  used_seed_ = has_seed;
  // Disable trials when testing to remove sources of nondeterminism.
  // WARNING: Do not add any field trials until after this check, or
  // else they will be incorrectly randomized during EG testing.
  if (tests_hook::DisableClientSideFieldTrials()) {
    return;
  }
}
