// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_UI_BUNDLED_MANAGEMENT_UTIL_H_
#define IOS_CHROME_BROWSER_POLICY_UI_BUNDLED_MANAGEMENT_UTIL_H_

#import <optional>
#import <string>

#import "ios/chrome/browser/policy/model/management_state.h"

class AuthenticationService;
class PrefService;
namespace signin {
class IdentityManager;
}  // namespace signin

// Returns whether the browser/user is managed, and the domain for policies
// through ManagementState.
ManagementState GetManagementState(signin::IdentityManager* identity_manager,
                                   AuthenticationService* auth_service,
                                   PrefService* prefs);

#endif  // IOS_CHROME_BROWSER_POLICY_UI_BUNDLED_MANAGEMENT_UTIL_H_
