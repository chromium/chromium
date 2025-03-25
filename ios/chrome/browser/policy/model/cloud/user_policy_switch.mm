// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/cloud/user_policy_switch.h"

namespace policy {

// TODO(crbug.com/320509638): Remove all of these, and the entire file.

bool IsUserPolicyEnabledForSigninOrSyncConsentLevel() {
  return false;
}

bool IsUserPolicyEnabledForSigninAndNoSyncConsentLevel() {
  return true;
}

bool IsAnyUserPolicyFeatureEnabled() {
  return true;
}

}  // namespace policy
