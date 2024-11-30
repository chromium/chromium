// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/model/features.h"

#import "base/metrics/field_trial_params.h"

namespace set_up_list {

BASE_FEATURE(kSetUpListInFirstRun,
             "SetUpListInFirstRun",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kSetUpListInFirstRunParam[] = "SetUpListInFirstRunParam";

FirstRunVariationType GetSetUpListInFirstRunVariation() {
  if (!base::FeatureList::IsEnabled(kSetUpListInFirstRun)) {
    return FirstRunVariationType::kDisabled;
  }
  return static_cast<FirstRunVariationType>(
      base::GetFieldTrialParamByFeatureAsInt(kSetUpListInFirstRun,
                                             kSetUpListInFirstRunParam, 1));
}

}  // namespace set_up_list
