// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_CLOUD_USER_POLICY_SWITCH_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_CLOUD_USER_POLICY_SWITCH_H_

namespace policy {

// True if User Policy is enabled for sign-in or sync consent levels.
bool IsUserPolicyEnabledForSigninOrSyncConsentLevel();

// True if User Policy is only enabled for sign-in consent level.
bool IsUserPolicyEnabledForSigninAndNoSyncConsentLevel();

// True if any User Policy feature is enabled.
bool IsAnyUserPolicyFeatureEnabled();

}  // namespace policy

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_CLOUD_USER_POLICY_SWITCH_H_
