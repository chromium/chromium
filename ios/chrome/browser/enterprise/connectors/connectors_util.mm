// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/connectors_util.h"

#import <optional>

#import "components/enterprise/browser/controller/browser_dm_token_storage.h"
#import "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#import "components/enterprise/common/proto/connectors.pb.h"
#import "components/enterprise/connectors/core/common.h"
#import "components/enterprise/connectors/core/reporting_constants.h"
#import "components/policy/core/common/cloud/affiliation.h"
#import "components/policy/core/common/cloud/cloud_policy_core.h"
#import "components/policy/core/common/cloud/cloud_policy_store.h"
#import "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#import "ios/chrome/browser/enterprise/common/util.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/components/enterprise/analysis/features.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_unsafe_resource_container.h"
#import "ios/web/common/user_agent.h"
#import "ios/web/public/web_client.h"

namespace enterprise_connectors {

base::DictValue GetContext(ProfileIOS* profile) {
  base::DictValue context;
  context.SetByDottedPath(
      "browser.userAgent",
      web::GetWebClient()->GetUserAgent(web::UserAgentType::MOBILE));

  if (!profile) {
    return context;
  }

  ProfileAttributesStorageIOS* storage = GetApplicationContext()
                                             ->GetProfileManager()
                                             ->GetProfileAttributesStorage();
  if (storage) {
    ProfileAttributesIOS attributes =
        storage->GetAttributesForProfileWithName(profile->GetProfileName());
    context.SetByDottedPath("profile.profileName", attributes.GetProfileName());
    context.SetByDottedPath("profile.gaiaEmail", attributes.GetUserName());
  }

  context.SetByDottedPath("profile.profilePath",
                          profile->GetStatePath().AsUTF8Unsafe());
  std::optional<std::string> client_id =
      GetUserClientId(profile->GetUserCloudPolicyManager());
  if (client_id) {
    context.SetByDottedPath("profile.clientId", *client_id);
  }
  std::optional<std::string> user_dm_token =
      enterprise::GetUserDmToken(profile->GetUserCloudPolicyManager());
  if (user_dm_token) {
    context.SetByDottedPath("profile.dmToken", *user_dm_token);
  }
  return context;
}

ClientMetadata GetContextAsClientMetadata(
    const std::string& profile_name,
    const base::FilePath& profile_path,
    policy::UserCloudPolicyManager* user_cloud_policy_manager) {
  ClientMetadata metadata;
  metadata.mutable_browser()->set_user_agent(
      web::GetWebClient()->GetUserAgent(web::UserAgentType::MOBILE));

  metadata.mutable_profile()->set_profile_path(profile_path.AsUTF8Unsafe());
  metadata.mutable_profile()->set_profile_name(profile_name);

  ProfileManagerIOS* manager = GetApplicationContext()->GetProfileManager();

  // The ProfileManagerIOS could be null in the tests.
  if (manager && manager->GetProfileAttributesStorage()) {
    ProfileAttributesIOS attributes =
        manager->GetProfileAttributesStorage()->GetAttributesForProfileWithName(
            profile_name);
    metadata.mutable_profile()->set_gaia_email(attributes.GetUserName());
  }

  std::optional<std::string> client_id =
      GetUserClientId(user_cloud_policy_manager);
  if (client_id) {
    metadata.mutable_profile()->set_client_id(*client_id);
  }

  std::optional<std::string> user_dm_token =
      enterprise::GetUserDmToken(user_cloud_policy_manager);
  if (user_dm_token) {
    metadata.mutable_profile()->set_dm_token(*user_dm_token);
  }

  return metadata;
}

std::optional<std::string> GetUserClientId(
    policy::UserCloudPolicyManager* user_cloud_policy_manager) {
  if (!user_cloud_policy_manager) {
    return std::nullopt;
  }

  policy::CloudPolicyStore* store = user_cloud_policy_manager->core()->store();
  if (!store || !store->has_policy()) {
    return std::nullopt;
  }

  const enterprise_management::PolicyData* policy_data = store->policy();
  if (!policy_data || !policy_data->has_device_id()) {
    return std::nullopt;
  }
  return policy_data->device_id();
}

base::flat_set<std::string> GetUserAffiliationIds(ProfileIOS* profile) {
  const enterprise_management::PolicyData* policy_data =
      enterprise::GetPolicyData(profile);
  if (!policy_data) {
    return {};
  }

  const auto& ids = policy_data->user_affiliation_ids();
  return {ids.begin(), ids.end()};
}

::chrome::cros::reporting::proto::UploadEventsRequest CreateUploadEventsRequest(
    ProfileIOS* profile) {
  ::chrome::cros::reporting::proto::UploadEventsRequest request;
  request.mutable_browser()->set_user_agent(
      web::GetWebClient()->GetUserAgent(web::UserAgentType::MOBILE));

  if (!profile) {
    return request;
  }

  request.mutable_profile()->set_profile_path(
      profile->GetStatePath().AsUTF8Unsafe());
  ProfileAttributesStorageIOS* storage = GetApplicationContext()
                                             ->GetProfileManager()
                                             ->GetProfileAttributesStorage();
  if (storage) {
    ProfileAttributesIOS attributes =
        storage->GetAttributesForProfileWithName(profile->GetProfileName());
    request.mutable_profile()->set_profile_name(attributes.GetProfileName());
    request.mutable_profile()->set_gaia_email(attributes.GetUserName());
  }

  std::optional<std::string> client_id =
      GetUserClientId(profile->GetUserCloudPolicyManager());
  if (client_id) {
    request.mutable_profile()->set_client_id(*client_id);
  }
  std::optional<std::string> user_dm_token =
      enterprise::GetUserDmToken(profile->GetUserCloudPolicyManager());
  if (user_dm_token) {
    request.mutable_profile()->set_dm_token(*user_dm_token);
  }

  return request;
}

bool IsEnterpriseUrlFilteringEnabled(EnterpriseRealTimeUrlCheckMode mode) {
  return mode ==
         EnterpriseRealTimeUrlCheckMode::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED;
}

bool IncludeDeviceInfo(bool per_profile, bool is_profile_affiliated) {
  if (!per_profile) {
    return true;
  }

  // An unmanaged browser shouldn't share its device info for privacy reasons.
  if (!policy::ChromeBrowserCloudManagementController::IsEnabled() ||
      !policy::BrowserDMTokenStorage::Get()->RetrieveDMToken().is_valid()) {
    return false;
  }

  // A managed device can share its info with the profile if they are
  // affiliated.
  return is_profile_affiliated;
}

bool IsDownloadConnectorEnabled(ConnectorsServiceBase* service) {
  CHECK(service);
  return base::FeatureList::IsEnabled(kEnableFileDownloadConnectorIOS) &&
         service->IsConnectorEnabled(AnalysisConnector::FILE_DOWNLOADED);
}

bool IsBulkDataEntryConnectorEnabled(ConnectorsServiceBase* service) {
  CHECK(service);
  return base::FeatureList::IsEnabled(kEnableBulkDataEntryConnectorIOS) &&
         service->IsConnectorEnabled(AnalysisConnector::BULK_DATA_ENTRY);
}

bool IsProfileAffilicated(ProfileIOS* profile) {
  return policy::IsAffiliated(GetUserAffiliationIds(profile),
                              GetApplicationContext()
                                  ->GetBrowserPolicyConnector()
                                  ->GetDeviceAffiliationIds());
}

RequestHandlerResultActionLevel ResultToActionLevel(
    const RequestHandlerResult& result) {
  switch (result.final_result) {
    case FinalContentAnalysisResult::SUCCESS:
      return RequestHandlerResultActionLevel::kAudit;
    case FinalContentAnalysisResult::WARNING:
      return RequestHandlerResultActionLevel::kWarn;
    case FinalContentAnalysisResult::LARGE_FILES:
    case FinalContentAnalysisResult::FAILURE:
    case FinalContentAnalysisResult::FAIL_CLOSED:
    case FinalContentAnalysisResult::CANCELLED:
      return RequestHandlerResultActionLevel::kBlock;
    case FinalContentAnalysisResult::KEPT_IN_MANAGED_CHROME:
    case FinalContentAnalysisResult::ENCRYPTED_FILES:
    case FinalContentAnalysisResult::FORCE_SAVE_TO_CLOUD:
      // Force Save to Cloud, Kept in Managed Chrome and Encrypted Files are
      // not supported on iOS.
      NOTREACHED();
  }
  return RequestHandlerResultActionLevel::kBlock;
}

}  // namespace enterprise_connectors
