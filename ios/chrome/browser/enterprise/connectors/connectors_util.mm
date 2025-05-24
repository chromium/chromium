// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/connectors_util.h"

#import <optional>

#import "base/feature_list.h"
#import "components/enterprise/browser/controller/browser_dm_token_storage.h"
#import "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#import "components/enterprise/common/proto/connectors.pb.h"
#import "components/enterprise/connectors/core/common.h"
#import "components/enterprise/connectors/core/reporting_constants.h"
#import "components/policy/core/common/cloud/affiliation.h"
#import "components/policy/core/common/cloud/cloud_policy_core.h"
#import "components/policy/core/common/cloud/cloud_policy_store.h"
#import "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#import "ios/chrome/browser/enterprise/connectors/features.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_unsafe_resource_container.h"
#import "ios/web/common/user_agent.h"
#import "ios/web/public/web_client.h"

namespace {

// Returns policy for the given `profile`. If failed to get policy returns
// nullptr.
const enterprise_management::PolicyData* GetPolicyData(ProfileIOS* profile) {
  if (!profile) {
    return nullptr;
  }

  auto* manager = profile->GetUserCloudPolicyManager();
  if (!manager) {
    return nullptr;
  }

  policy::CloudPolicyStore* store = manager->core()->store();
  if (!store || !store->has_policy()) {
    return nullptr;
  }

  return store->policy();
}

}  // namespace

namespace enterprise_connectors {

base::Value::Dict GetContext(ProfileIOS* profile) {
  base::Value::Dict context;
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
  std::optional<std::string> client_id = GetUserClientId(profile);
  if (client_id) {
    context.SetByDottedPath("profile.clientId", *client_id);
  }
  std::optional<std::string> user_dm_token = GetUserDmToken(profile);
  if (user_dm_token) {
    context.SetByDottedPath("profile.dmToken", *user_dm_token);
  }
  return context;
}

ClientMetadata GetContextAsClientMetadata(ProfileIOS* profile) {
  ClientMetadata metadata;
  metadata.mutable_browser()->set_user_agent(
      web::GetWebClient()->GetUserAgent(web::UserAgentType::MOBILE));

  if (!profile) {
    return metadata;
  }

  ProfileAttributesStorageIOS* storage = GetApplicationContext()
                                             ->GetProfileManager()
                                             ->GetProfileAttributesStorage();
  if (storage) {
    ProfileAttributesIOS attributes =
        storage->GetAttributesForProfileWithName(profile->GetProfileName());
    metadata.mutable_profile()->set_profile_name(attributes.GetProfileName());
    metadata.mutable_profile()->set_gaia_email(attributes.GetUserName());
  }

  metadata.mutable_profile()->set_profile_path(
      profile->GetStatePath().AsUTF8Unsafe());
  std::optional<std::string> client_id = GetUserClientId(profile);
  if (client_id) {
    metadata.mutable_profile()->set_client_id(*client_id);
  }

  std::optional<std::string> user_dm_token = GetUserDmToken(profile);
  if (user_dm_token) {
    metadata.mutable_profile()->set_dm_token(*user_dm_token);
  }

  return metadata;
}

std::optional<std::string> GetUserDmToken(ProfileIOS* profile) {
  if (!profile) {
    return std::nullopt;
  }
  const enterprise_management::PolicyData* policy_data = GetPolicyData(profile);
  if (!policy_data || !policy_data->has_request_token()) {
    return std::nullopt;
  }
  return policy_data->request_token();
}

std::optional<std::string> GetUserClientId(ProfileIOS* profile) {
  if (!profile) {
    return std::nullopt;
  }

  const enterprise_management::PolicyData* policy_data = GetPolicyData(profile);
  if (!policy_data || !policy_data->has_device_id()) {
    return std::nullopt;
  }
  return policy_data->device_id();
}

base::flat_set<std::string> GetUserAffiliationIds(ProfileIOS* profile) {
  const enterprise_management::PolicyData* policy_data = GetPolicyData(profile);
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

  std::optional<std::string> client_id = GetUserClientId(profile);
  if (client_id) {
    request.mutable_profile()->set_client_id(*client_id);
  }
  std::optional<std::string> user_dm_token = GetUserDmToken(profile);
  if (user_dm_token) {
    request.mutable_profile()->set_dm_token(*user_dm_token);
  }

  return request;
}

bool IsEnterpriseUrlFilteringEnabled(EnterpriseRealTimeUrlCheckMode mode) {
  if (!base::FeatureList::IsEnabled(
          enterprise_connectors::kIOSEnterpriseRealtimeUrlFiltering)) {
    return false;
  }

  return mode ==
         EnterpriseRealTimeUrlCheckMode::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED;
}

bool IncludeDeviceInfo(ProfileIOS* profile, bool per_profile) {
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
  return policy::IsAffiliated(GetUserAffiliationIds(profile),
                              GetApplicationContext()
                                  ->GetBrowserPolicyConnector()
                                  ->GetDeviceAffiliationIds());
}

}  // namespace enterprise_connectors
