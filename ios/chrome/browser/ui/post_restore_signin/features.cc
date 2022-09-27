// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/post_restore_signin/features.h"

#import "base/feature_list.h"
#import "base/metrics/field_trial_params.h"

namespace post_restore_signin {
namespace features {

BASE_FEATURE(kIOSNewPostRestoreExperience,
             "IOSNewPostRestoreExperience",
             base::FEATURE_DISABLED_BY_DEFAULT);
const char kIOSNewPostRestoreExperienceParam[] =
    "ios-new-post-restore-experience";

PostRestoreSignInType CurrentPostRestoreSignInType() {
  if (base::FeatureList::IsEnabled(kIOSNewPostRestoreExperience))
    return base::GetFieldTrialParamByFeatureAsBool(
               kIOSNewPostRestoreExperience, kIOSNewPostRestoreExperienceParam,
               false)
               ? PostRestoreSignInType::kAlert
               : PostRestoreSignInType::kFullscreen;

  return PostRestoreSignInType::kDisabled;
}

}  // namespace features
}  // namespace post_restore_signin
