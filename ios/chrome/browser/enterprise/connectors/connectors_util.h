// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_UTIL_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_UTIL_H_

#import "base/containers/flat_set.h"
#import "base/values.h"
#import "components/enterprise/common/proto/connectors.pb.h"
#import "components/enterprise/common/proto/upload_request_response.pb.h"
#import "components/enterprise/connectors/core/connectors_service_base.h"
#import "components/enterprise/connectors/core/reporting_constants.h"

class ProfileIOS;

namespace policy {
class UserCloudPolicyManager;
}

namespace enterprise_connectors {

enum class RequestHandlerResultActionLevel {
  // If text or image is empty then action level is not scan.
  kNotScan = 0,
  kAudit = 1,
  kWarn = 2,
  kBlock = 3,
};

// Fetches additional information that is common to every event. Fetches and
// returns corresponding info to a Device, Browser and Profile protos defined in
// google3/google/internal/chrome/reporting/v1/chromereporting.proto.
//
// TODO(crbug.com/403335734): Deprecated this method once the migration from
// using dictionary to proto for the reporting event is done.
base::DictValue GetContext(ProfileIOS* profile);

// Fetches the same information as GetContext, but in a protobuf instead of a
// Value.
ClientMetadata GetContextAsClientMetadata(
    const std::string& profile_name,
    const base::FilePath& profile_path,
    policy::UserCloudPolicyManager* user_cloud_policy_manager);

// Returns client id for a given `UserCloudPolicyManager` if it exists.
std::optional<std::string> GetUserClientId(
    policy::UserCloudPolicyManager* user_cloud_policy_manager);

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

// Returns whether device info should be reported based on policy scope and
// affiliation.
bool IncludeDeviceInfo(bool per_profile, bool is_profile_affiliated);

// Returns whether the download connector feature flag is turned on and the
// connector is enabled.
bool IsDownloadConnectorEnabled(ConnectorsServiceBase* service);

// Returns whether the bulk data entry connector feature flag is turned on and
// the connector is enabled.
bool IsBulkDataEntryConnectorEnabled(ConnectorsServiceBase* service);

// Returns true if the profile and browser are managed by the same customer
// (affiliated). This is determined by comparing affiliation IDs obtained in the
// policy fetching response. If either policies has no affiliation IDs, this
// function returns false.
bool IsProfileAffilicated(ProfileIOS* profile);

// Map the `FinalContentAnalysisResult` to action level: kAudit, kWarn and
// kBlock.
RequestHandlerResultActionLevel ResultToActionLevel(
    const RequestHandlerResult& result);

}  // namespace enterprise_connectors

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_UTIL_H_
