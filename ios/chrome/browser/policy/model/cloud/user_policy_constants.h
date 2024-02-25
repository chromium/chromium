// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_CLOUD_USER_POLICY_CONSTANTS_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_CLOUD_USER_POLICY_CONSTANTS_H_

#import "base/feature_list.h"

namespace policy {

// Enables User Policy for signed-in or signed-in+sync users.
BASE_DECLARE_FEATURE(kUserPolicyForSigninOrSyncConsentLevel);

// Enables User Policy for signed-in only and no sync users.
BASE_DECLARE_FEATURE(kUserPolicyForSigninAndNoSyncConsentLevel);

// Show the User Policy notification at startup iff needed.
BASE_DECLARE_FEATURE(kShowUserPolicyNotificationAtStartupIfNeeded);

}  // namespace policy

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_CLOUD_USER_POLICY_CONSTANTS_H_
