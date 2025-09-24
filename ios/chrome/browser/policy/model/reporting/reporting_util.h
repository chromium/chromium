// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_REPORTING_UTIL_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_REPORTING_UTIL_H_

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

}  // namespace enterprise_reporting

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_REPORTING_UTIL_H_
