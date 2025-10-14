// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_COMMON_UTIL_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_COMMON_UTIL_H_

#import <optional>
#import <string>

#import "components/policy/core/common/policy_types.h"
#import "components/policy/proto/device_management_backend.pb.h"

class ProfileIOS;

namespace enterprise {

// Returns the PolicyData associated with the given `profile`, or nullptr if it
// cannot be retrieved.
const enterprise_management::PolicyData* GetPolicyData(ProfileIOS* profile);

// Returns the browser's device management token, if it exists.
std::optional<std::string> GetBrowserDmToken();

// Returns User DMToken for a given `profile` if:
// * `profile` is NOT incognito profile.
// * `profile` is NOT sign-in screen profile
// * user corresponding to a `profile` is managed.
// Otherwise returns empty optional. More about DMToken:
// go/dmserver-domain-model#dmtoken.
std::optional<std::string> GetUserDmToken(ProfileIOS* profile);

// Returns the CBCM domain or profile domain for the given policy::PolicyScope
// and ProfileIOS.
std::string GetManagementDomain(std::optional<policy::PolicyScope> policy_scope,
                                ProfileIOS* profile);

}  // namespace enterprise

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_COMMON_UTIL_H_
