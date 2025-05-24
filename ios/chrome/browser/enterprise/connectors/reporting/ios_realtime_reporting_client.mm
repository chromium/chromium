// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/reporting/ios_realtime_reporting_client.h"

#import "base/metrics/histogram_functions.h"
#import "base/strings/escape.h"
#import "base/timer/timer.h"
#import "components/enterprise/browser/controller/browser_dm_token_storage.h"
#import "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#import "components/enterprise/browser/identifiers/profile_id_service.h"
#import "components/policy/core/common/cloud/affiliation.h"
#import "components/policy/core/common/cloud/cloud_policy_client.h"
#import "components/policy/core/common/cloud/cloud_policy_constants.h"
#import "components/policy/core/common/cloud/dm_token.h"
#import "components/policy/core/common/cloud/reporting_job_configuration_base.h"
#import "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#import "components/profile_metrics/browser_profile_type.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/enterprise/connectors/connectors_service_factory.h"
#import "ios/chrome/browser/enterprise/connectors/connectors_util.h"
#import "ios/chrome/browser/enterprise/identifiers/profile_id_service_factory_ios.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/policy/model/profile_policy_connector.h"
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

void IOSRealtimeReportingClient::SetBrowserCloudPolicyClientForTesting(
    policy::CloudPolicyClient* client) {
  if (client == nullptr && browser_client_) {
    browser_client_->RemoveObserver(this);
  }

  browser_client_ = client;
  if (browser_client_) {
    browser_client_->AddObserver(this);
  }
}

void IOSRealtimeReportingClient::SetProfileCloudPolicyClientForTesting(
    policy::CloudPolicyClient* client) {
  if (client == nullptr && profile_client_) {
    profile_client_->RemoveObserver(this);
  }

  profile_client_ = client;
  if (profile_client_) {
    profile_client_->AddObserver(this);
  }
}

void IOSRealtimeReportingClient::SetIdentityManagerForTesting(
    signin::IdentityManager* identity_manager) {
  identity_manager_ = identity_manager;
}

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

std::optional<ReportingSettings>
IOSRealtimeReportingClient::GetReportingSettings() {
  auto* service = ConnectorsServiceFactory::GetForProfile(profile_);
  if (!service) {
    return std::nullopt;
  }

  return service->GetReportingSettings();
}

void IOSRealtimeReportingClient::ReportRealtimeEvent(
    const std::string& name,
    const ReportingSettings& settings,
    base::Value::Dict event) {
  ReportEventWithTimestampDeprecated(name, settings, std::move(event),
                                     base::Time::Now(),
                                     /*include_profile_user_name=*/true);
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
  if (profile_client_) {
    auto* profile_id_service =
        enterprise::ProfileIdServiceFactoryIOS::GetForProfile(profile_);
    if (profile_id_service && profile_id_service->GetProfileId().has_value()) {
      return profile_id_service->GetProfileId().value();
    }
    return std::string();
  }
  return profile_->GetStatePath().AsUTF8Unsafe();
}

std::string IOSRealtimeReportingClient::GetBrowserClientId() {
  return policy::BrowserDMTokenStorage::Get()->RetrieveClientId();
}

bool IOSRealtimeReportingClient::ShouldIncludeDeviceInfo(bool per_profile) {
  return IncludeDeviceInfo(profile_, per_profile);
}

void IOSRealtimeReportingClient::UploadCallbackDeprecated(
    base::Value::Dict event_wrapper,
    bool per_profile,
    policy::CloudPolicyClient* client,
    EnterpriseReportingEventType eventType,
    policy::CloudPolicyClient::Result upload_result) {
  // TODO(crbug.com/256553070): Do not crash if the client is unregistered.
  CHECK(!upload_result.IsClientNotRegisteredError());

  if (upload_result.IsSuccess()) {
    base::UmaHistogramEnumeration("Enterprise.ReportingEventUploadSuccess",
                                  eventType);
  } else {
    base::UmaHistogramEnumeration("Enterprise.ReportingEventUploadFailure",
                                  eventType);
  }
}

void IOSRealtimeReportingClient::UploadCallback(
    ::chrome::cros::reporting::proto::UploadEventsRequest request,
    bool per_profile,
    policy::CloudPolicyClient* client,
    EnterpriseReportingEventType eventType,
    policy::CloudPolicyClient::Result upload_result) {
  if (upload_result.IsSuccess()) {
    base::UmaHistogramEnumeration("Enterprise.ReportingEventUploadSuccess",
                                  eventType);
  } else {
    base::UmaHistogramEnumeration("Enterprise.ReportingEventUploadFailure",
                                  eventType);
  }
}

base::Value::Dict IOSRealtimeReportingClient::GetContext() {
  return ::enterprise_connectors::GetContext(profile_);
}

::chrome::cros::reporting::proto::UploadEventsRequest
IOSRealtimeReportingClient::CreateUploadEventsRequest() {
  return ::enterprise_connectors::CreateUploadEventsRequest(profile_);
}

void IOSRealtimeReportingClient::OnClientError(
    policy::CloudPolicyClient* client) {
  // This is the status set when the server returned 403, which is what the
  // reporting server returns when the customer is not allowed to report events.
  if (client->last_dm_status() ==
      policy::DM_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED) {
    // This could happen if a second event was fired before the first one
    // returned an error.
    if (!rejected_dm_token_timers_.contains(client->dm_token())) {
      rejected_dm_token_timers_[client->dm_token()] =
          std::make_unique<base::OneShotTimer>();
      rejected_dm_token_timers_[client->dm_token()]->Start(
          FROM_HERE, base::Hours(24),
          base::BindOnce(
              &IOSRealtimeReportingClient::RemoveDmTokenFromRejectedSet,
              AsWeakPtrImpl(), client->dm_token()));
    }
  }
}

void IOSRealtimeReportingClient::RemoveDmTokenFromRejectedSet(
    const std::string& dm_token) {
  rejected_dm_token_timers_.erase(dm_token);
}

base::WeakPtr<RealtimeReportingClientBase>
IOSRealtimeReportingClient::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace enterprise_connectors
