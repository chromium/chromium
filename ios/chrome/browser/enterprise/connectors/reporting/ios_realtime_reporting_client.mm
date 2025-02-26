// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/reporting/ios_realtime_reporting_client.h"

#import "base/metrics/histogram_functions.h"
#import "base/strings/escape.h"
#import "components/enterprise/browser/controller/browser_dm_token_storage.h"
#import "components/policy/core/common/cloud/cloud_policy_client.h"
#import "components/policy/core/common/cloud/reporting_job_configuration_base.h"
#import "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#import "components/profile_metrics/browser_profile_type.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace enterprise_connectors {

IOSRealtimeReportingClient::IOSRealtimeReportingClient(ProfileIOS* profile)
    : RealtimeReportingClientBase(
          GetApplicationContext()
              ->GetBrowserPolicyConnector()
              ->device_management_service(),
          GetApplicationContext()->GetSharedURLLoaderFactory()),
      profile_(profile) {
  identity_manager_ = IdentityManagerFactory::GetForProfile(profile_);
}

IOSRealtimeReportingClient::~IOSRealtimeReportingClient() = default;

std::pair<std::string, policy::CloudPolicyClient*>
IOSRealtimeReportingClient::InitProfileReportingClient(
    const std::string& dm_token) {
  policy::CloudPolicyManager* policy_manager =
      profile_->GetUserCloudPolicyManager();
  if (!policy_manager || !policy_manager->core() ||
      !policy_manager->core()->client()) {
    return {GetProfilePolicyClientDescription(), nullptr};
  }

  profile_private_client_ = std::make_unique<policy::CloudPolicyClient>(
      policy_manager->core()->client()->service(),
      GetApplicationContext()->GetSharedURLLoaderFactory(),
      policy::CloudPolicyClient::DeviceDMTokenCallback());
  policy::CloudPolicyClient* client = profile_private_client_.get();

  client->SetupRegistration(dm_token,
                            policy_manager->core()->client()->client_id(),
                            /*user_affiliation_ids*/ {});

  return {GetProfilePolicyClientDescription(), client};
}

std::string IOSRealtimeReportingClient::GetProfileUserName() {
  if (!username_.empty()) {
    return username_;
  }
  username_ = identity_manager_
                  ? identity_manager_
                        ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
                        .email
                  : std::string();

  return username_;
}

std::string IOSRealtimeReportingClient::GetProfileIdentifier() {
  // TODO(crbug.com/394097677): Implement this.
  return profile_->GetStatePath().AsUTF8Unsafe();
}

std::string IOSRealtimeReportingClient::GetBrowserClientId() {
  return policy::BrowserDMTokenStorage::Get()->RetrieveClientId();
}

bool IOSRealtimeReportingClient::ShouldIncludeDeviceInfo(bool per_profile) {
  // TODO(crbug.com/394097677): implement this.
  if (!per_profile) {
    return true;
  }

  return false;
}

void IOSRealtimeReportingClient::UploadCallbackDeprecated(
    base::Value::Dict event_wrapper,
    bool per_profile,
    policy::CloudPolicyClient* client,
    EnterpriseReportingEventType eventType,
    policy::CloudPolicyClient::Result upload_result) {}

void IOSRealtimeReportingClient::UploadCallback(
    ::chrome::cros::reporting::proto::UploadEventsRequest request,
    bool per_profile,
    policy::CloudPolicyClient* client,
    EnterpriseReportingEventType eventType,
    policy::CloudPolicyClient::Result upload_result) {
  // TODO(crbug.com/394097677): Add report event to safe_browsing.
  if (upload_result.IsSuccess()) {
    base::UmaHistogramEnumeration("Enterprise.ReportingEventUploadSuccess",
                                  eventType);
  } else {
    base::UmaHistogramEnumeration("Enterprise.ReportingEventUploadFailure",
                                  eventType);
  }
}

base::Value::Dict IOSRealtimeReportingClient::GetContext() {
  // TODO(crbug.com/394097677): Implement this.
  base::Value::Dict context;
  return context;
}

::chrome::cros::reporting::proto::UploadEventsRequest
IOSRealtimeReportingClient::CreateUploadEventsRequest() {
  ::chrome::cros::reporting::proto::UploadEventsRequest request;

  // TODO(crbug.com/394098919): Implement this before starting reporting events.
  return request;
}

base::WeakPtr<RealtimeReportingClientBase>
IOSRealtimeReportingClient::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace enterprise_connectors
