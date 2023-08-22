// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POLICY_USER_POLICY_UTIL_H_
#define IOS_CHROME_BROWSER_UI_POLICY_USER_POLICY_UTIL_H_

class AuthenticationService;
class PrefService;

// Returns true if a notification as to be shown for User Policy.
bool IsUserPolicyNotificationNeeded(AuthenticationService* authService,
                                    PrefService* prefService);

// Returns true if user policies can be fetched.
bool CanFetchUserPolicy(AuthenticationService* authService,
                        PrefService* prefService);

#endif  // IOS_CHROME_BROWSER_UI_POLICY_USER_POLICY_UTIL_H_
