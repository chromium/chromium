// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/cloud/user_policy_switch.h"

#import "base/feature_list.h"
#import "ios/chrome/browser/policy/model/cloud/user_policy_constants.h"

namespace policy {

bool IsUserPolicyEnabledForSigninOrSyncConsentLevel() {
  return base::FeatureList::IsEnabled(kUserPolicyForSigninOrSyncConsentLevel);
}

bool IsUserPolicyEnabledForSigninAndNoSyncConsentLevel() {
  return base::FeatureList::IsEnabled(
      kUserPolicyForSigninAndNoSyncConsentLevel);
}

bool IsAnyUserPolicyFeatureEnabled() {
  return IsUserPolicyEnabledForSigninOrSyncConsentLevel() ||
         IsUserPolicyEnabledForSigninAndNoSyncConsentLevel();
}

}  // namespace policy
