// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_UI_BUNDLED_POLICY_DOMAIN_UTIL_H_
#define IOS_CHROME_BROWSER_POLICY_UI_BUNDLED_POLICY_DOMAIN_UTIL_H_

#include <optional>
#include <string>

class AuthenticationService;
class PrefService;
namespace signin {
class IdentityManager;
}  // namespace signin

// Returns the domain of the machine level cloud policy. Returns std::nullopt
// if the domain cannot be retrieved (eg. because there are no machine level
// policies).
std::optional<std::string> GetMachineLevelPolicyDomain();

// Returns the domain of the user cloud policy. Returns std::nullopt if the
// domain cannot be retrieved (eg. because there is no user policy).
std::optional<std::string> GetUserPolicyDomain(
    signin::IdentityManager* identity_manager,
    AuthenticationService* auth_service,
    PrefService* prefs);

#endif  // IOS_CHROME_BROWSER_POLICY_UI_BUNDLED_POLICY_DOMAIN_UTIL_H_
