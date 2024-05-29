// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_POLICY_EARL_GREY_UTILS_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_POLICY_EARL_GREY_UTILS_H_

#import <string>

namespace base {
class Value;
}

namespace policy_test_utils {

// Returns a JSON-encoded representation of the value for the given
// `policy_key`. Looks for the policy in the platform policy provider under the
// CHROME policy namespace.
std::string GetValueForPlatformPolicy(const std::string& policy_key);

// Sets the boolean policy corresponding to `policy_key` to the given boolean
// value, and removes previous policies.
void SetPolicy(bool enabled, const std::string& policy_key);
// Merges the boolean policy corresponding to `policy_key` to the existing
// policies with its value sets to `enabled`.
void MergePolicy(bool enabled, const std::string& policy_key);

// Sets the integer policy corresponding to `policy_key` to the given integer
// value, and removes previous policies.
void SetPolicy(int value, const std::string& policy_key);
// Merges the integer policy corresponding to `policy_key` to the existing
// policies with its value sets to `value`.
void MergePolicy(int value, const std::string& policy_key);

// Sets string policy corresponding to `policy_key` to the given string
// value, and removes previous policies.
void SetPolicyWithStringValue(const std::string& value,
                              const std::string& policy_key);
// Merges the string policy corresponding to `policy_key` to the existing
// policies with its value sets to `value`.
void MergePolicyWithStringValue(const std::string& value,
                                const std::string& policy_key);

// Sets the value of the policy with the `policy_key` key to the given value.
// The value must be serialized as a JSON string.
// Prefer using the other type-specific helpers instead of this generic helper
// if possible.
void SetPolicy(const std::string& json_value, const std::string& policy_key);

// Sets the value of the policy with the `policy_key` key to the given value.
// The value must be wrapped in a `base::Value`.
// Prefer using the other type-specific helpers instead of this generic helper
// if possible.
void SetPolicy(base::Value value, const std::string& policy_key);

// Clears all policy values.
void ClearPolicies();

}  // namespace policy_test_utils

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_POLICY_EARL_GREY_UTILS_H_
