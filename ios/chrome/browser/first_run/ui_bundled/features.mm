// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/features.h"

#import "base/metrics/field_trial_params.h"

namespace first_run {

BASE_FEATURE(kUpdatedFirstRunSequence,
             "UpdatedFirstRunSequence",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kUpdatedFirstRunSequenceParam[] = "updated-first-run-sequence-param";

UpdatedFRESequenceVariationType GetUpdatedFRESequenceVariation() {
  if (!base::FeatureList::IsEnabled(kUpdatedFirstRunSequence)) {
    return UpdatedFRESequenceVariationType::kDisabled;
  }
  return static_cast<UpdatedFRESequenceVariationType>(
      base::GetFieldTrialParamByFeatureAsInt(kUpdatedFirstRunSequence,
                                             kUpdatedFirstRunSequenceParam, 1));
}

}  // namespace first_run
