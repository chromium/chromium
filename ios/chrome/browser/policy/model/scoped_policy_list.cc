// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/policy/model/scoped_policy_list.h"
#import "ios/chrome/browser/policy/model/policy_earl_grey_utils.h"

ScopedPolicyList::ScopedPolicyList() {}

ScopedPolicyList::~ScopedPolicyList() {
  Reset();
}

void ScopedPolicyList::SetPolicy(int value, const std::string& policy_key) {
  // First check if this `policy` has been set using this object before.
  // If not, store the current policy value before changing it.
  const auto stored_original_value =
      original_value_for_policy_.find(policy_key);
  if (stored_original_value == original_value_for_policy_.end()) {
    const auto original_value =
        policy_test_utils::GetValueForPlatformPolicy(policy_key);
    original_value_for_policy_.insert({policy_key, original_value});
  }

  policy_test_utils::SetPolicy(value, policy_key);
}

void ScopedPolicyList::Reset() {
  for (const auto& policy_key_value : original_value_for_policy_) {
    const auto& policy_key = policy_key_value.first;
    const auto& original_value = policy_key_value.second;
    policy_test_utils::SetPolicy(original_value, policy_key);
  }
}
