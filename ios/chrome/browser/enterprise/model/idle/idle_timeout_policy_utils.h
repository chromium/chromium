// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_MODEL_IDLE_IDLE_TIMEOUT_POLICY_UTILS_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_MODEL_IDLE_IDLE_TIMEOUT_POLICY_UTILS_H_

#import "ios/chrome/browser/signin/model/authentication_service.h"

class PrefService;

namespace enterprise_idle {

struct ActionSet {
  bool clear = false;    // True if any of ActionType::kClear* is present.
  bool close = false;    // True if ActionType::kCloseTabs is present.
  bool signout = false;  // True if any of ActionType::kSignOut is present.
};

// Returns the action set based on the value of `IdleTimeoutActions`.
// The action set only contains `signout` if the user is signed in.
ActionSet GetActionSet(PrefService* prefs, AuthenticationService* auth_service);

}  // namespace enterprise_idle

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_MODEL_IDLE_IDLE_TIMEOUT_POLICY_UTILS_H_
