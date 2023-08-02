// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/cloud/user_policy_switch.h"

#import "base/feature_list.h"
#import "ios/chrome/browser/policy/cloud/user_policy_constants.h"

namespace policy {

bool IsUserPolicyEnabled() {
  return base::FeatureList::IsEnabled(kUserPolicy);
}

}  // namespace policy
