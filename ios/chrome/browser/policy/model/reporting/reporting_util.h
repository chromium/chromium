// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_REPORTING_UTIL_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_REPORTING_UTIL_H_

#import <string>
#import <string_view>

#import "components/policy/proto/device_management_backend.pb.h"
#import "ios/chrome/browser/policy/model/profile_policy_connector.h"

class ProfileIOS;

namespace enterprise_reporting {

// Returns true if the profile and browser are managed by the same customer
// (affiliated). This is determined by comparing affiliation IDs obtained in the
// policy fetching response. If either policies has no affiliation IDs, this
// function returns false.
bool IsProfileAffiliated(ProfileIOS* profile);

// Returns more details for unaffiliated profile. Only used for unaffiliated
// profile.
enterprise_management::AffiliationState::UnaffiliationReason
GetUnaffiliatedReason(ProfileIOS* profile);

// Transforms the profile path & name into a stable identifier that looks like
// a path to an absolute file.
//
// This transformation is needed, because the raw profile path changes whenever
// there's a browser update.
//
// i.e., if the profile path looks like this:
// /var/mobile/Containers/Data/Application/AA07B6E9-5BE0-40A7-8E6B-8221597D0728/Library/Application
// Support/Chromium/ee0ffa42-225b-4ee8-ae4e-00baa3dc007c
//
// The "AA07B..." part changes whenever you install a new version of Chromium,
// so it changes from one version to the next.
std::string SanitizeProfilePath(std::string_view profile_name);

}  // namespace enterprise_reporting

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_REPORTING_UTIL_H_
