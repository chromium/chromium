// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_UTIL_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_UTIL_H_

#import "base/containers/flat_set.h"
#import "base/values.h"
#import "components/enterprise/common/proto/connectors.pb.h"
#import "components/enterprise/common/proto/upload_request_response.pb.h"
#import "components/enterprise/connectors/core/reporting_constants.h"

class ProfileIOS;

namespace enterprise_connectors {

// Fetches additional information that is common to every event. Fetches and
// returns corresponding info to a Device, Browser and Profile protos defined in
// google3/google/internal/chrome/reporting/v1/chromereporting.proto.
//
// TODO(crbug.com/403335734): Deprecated this method once the migration from
// using dictionary to proto for the reporting event is done.
base::Value::Dict GetContext(ProfileIOS* profile);

// Fetches the same information as GetContext, but in a protobuf instead of a
// Value.
ClientMetadata GetContextAsClientMetadata(ProfileIOS* profile);

// Returns User DMToken or client id for a given `profile` if:
// * `profile` is NOT incognito profile.
// * `profile` is NOT sign-in screen profile
// * user corresponding to a `profile` is managed.
// Otherwise returns empty optional. More about DMToken:
// go/dmserver-domain-model#dmtoken.
std::optional<std::string> GetUserDmToken(ProfileIOS* profile);
std::optional<std::string> GetUserClientId(ProfileIOS* profile);

// Returns affiliation IDs contained in the PolicyData corresponding to the
// profile.
base::flat_set<std::string> GetUserAffiliationIds(ProfileIOS* profile);

// Creates and returns an UploadEventsRequest proto with the Device, Browser and
// Profile fields set.
::chrome::cros::reporting::proto::UploadEventsRequest CreateUploadEventsRequest(
    ProfileIOS* profile);

// Helper that checks feature flags and policies to determine if Enterprise Url
// Filtering is enabled.
bool IsEnterpriseUrlFilteringEnabled(EnterpriseRealTimeUrlCheckMode mode);

// Returns whether device info should be reported for the profile.
bool IncludeDeviceInfo(ProfileIOS* profile, bool per_profile);

}  // namespace enterprise_connectors

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_UTIL_H_
