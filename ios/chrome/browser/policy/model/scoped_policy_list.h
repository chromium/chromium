// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_SCOPED_POLICY_LIST_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_SCOPED_POLICY_LIST_H_

#include <string>
#include <unordered_map>

// `ScopedPolicyList` modifies the block policies through `PolicyAppInterface`
// and resets these policies to their original values when it goes out of scope.
class ScopedPolicyList {
 public:
  ScopedPolicyList();
  ~ScopedPolicyList();

  // Sets the value of the policy with the `policy_key` to the given integer
  // value.
  void SetPolicy(int value, const std::string& policy_key);

  // Reset all policies which have been set to their original value
  // i.e. the value they had when they were first set.
  void Reset();

 private:
  std::unordered_map<std::string, std::string> original_value_for_policy_;
};

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_SCOPED_POLICY_LIST_H_
