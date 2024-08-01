// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_UI_BUNDLED_USER_POLICY_UTIL_H_
#define IOS_CHROME_BROWSER_POLICY_UI_BUNDLED_USER_POLICY_UTIL_H_

class AuthenticationService;
class PrefService;

namespace policy {
class UserCloudPolicyManager;
}

// Returns true if a notification as to be shown for User Policy.
// `user_policy_provider` can be null in which case false will be returned.
bool IsUserPolicyNotificationNeeded(
    AuthenticationService* authService,
    PrefService* prefService,
    const policy::UserCloudPolicyManager* user_policy_manager);

// Returns true if user policies can be fetched.
bool CanFetchUserPolicy(AuthenticationService* authService,
                        PrefService* prefService);

#endif  // IOS_CHROME_BROWSER_POLICY_UI_BUNDLED_USER_POLICY_UTIL_H_
