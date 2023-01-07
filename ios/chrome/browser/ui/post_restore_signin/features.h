// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POST_RESTORE_SIGNIN_FEATURES_H_
#define IOS_CHROME_BROWSER_UI_POST_RESTORE_SIGNIN_FEATURES_H_

#include "base/feature_list.h"

namespace post_restore_signin {
namespace features {

// Feature flag and param that enables the new post device restore experience
// which will prompt the user to sign-in again.
BASE_DECLARE_FEATURE(kIOSNewPostRestoreExperience);
extern const char kIOSNewPostRestoreExperienceParam[];

// Post Restore Sign-in Options.
enum class PostRestoreSignInType {
  // Post Restore Sign-in enabled with fullscreen, FRE-like promo.
  kFullscreen = 0,
  // Post Restore Sign-in enabled with native iOS alert-like promo.
  kAlert = 1,
  // Post Restore Sign-in disabled.
  kDisabled = 2,
};

// Returns the current PostRestoreSignInType according to the feature flag and
// experiment `kIOSNewPostRestoreExperience`.
PostRestoreSignInType CurrentPostRestoreSignInType();

}  // namespace features
}  // namespace post_restore_signin

#endif  // IOS_CHROME_BROWSER_UI_POST_RESTORE_SIGNIN_FEATURES_H_
