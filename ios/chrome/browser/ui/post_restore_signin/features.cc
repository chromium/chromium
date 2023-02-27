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
             base::FEATURE_ENABLED_BY_DEFAULT);
const char kIOSNewPostRestoreExperienceParam[] =
    "ios-new-post-restore-experience";

PostRestoreSignInType CurrentPostRestoreSignInType() {
  if (base::FeatureList::IsEnabled(kIOSNewPostRestoreExperience)) {
    return PostRestoreSignInType::kAlert;
  }

  return PostRestoreSignInType::kDisabled;
}

}  // namespace features
}  // namespace post_restore_signin
