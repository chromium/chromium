// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ios_chrome_field_trials.h"

#include "base/check.h"
#include "base/path_service.h"
#include "components/metrics/persistent_histograms.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/chrome_paths.h"
#include "ios/chrome/browser/ui/first_run/fre_field_trial.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

void IOSChromeFieldTrials::SetUpFieldTrials() {
  // Persistent histograms must be enabled as soon as possible.
  base::FilePath user_data_dir;
  if (base::PathService::Get(ios::DIR_USER_DATA, &user_data_dir)) {
    InstantiatePersistentHistograms(user_data_dir);
  }
}

void IOSChromeFieldTrials::SetUpFeatureControllingFieldTrials(
    bool has_seed,
    const base::FieldTrial::EntropyProvider* low_entropy_provider,
    base::FeatureList* feature_list) {
  // Add code here to enable field trials that are active at first run.
  // See http://crrev/c/1128269 for an example.
  // Note: On iOS, the |low_entropy_provider| is guaranteed to be non-null.
  DCHECK(low_entropy_provider);
  fre_field_trial::Create(*low_entropy_provider, feature_list,
                          GetApplicationContext()->GetLocalState());
}
