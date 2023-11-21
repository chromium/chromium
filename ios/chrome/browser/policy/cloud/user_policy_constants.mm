// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/cloud/user_policy_constants.h"

namespace policy {

BASE_FEATURE(kUserPolicyForSigninOrSyncConsentLevel,
             "UserPolicyForSigninOrSyncConsentLevel",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kUserPolicyForSigninAndNoSyncConsentLevel,
             "UserPolicyForSigninAndNoSyncConsentLevel",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kShowUserPolicyNotificationAtStartupIfNeeded,
             "ShowUserPolicyNotificationAtStartupIfNeeded",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace policy
