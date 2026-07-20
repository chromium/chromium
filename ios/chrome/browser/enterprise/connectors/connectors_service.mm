// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/connectors_service.h"

#import "base/types/expected.h"
#import "components/enterprise/browser/controller/browser_dm_token_storage.h"
#import "components/enterprise/connectors/core/common.h"
#import "components/enterprise/connectors/core/connectors_prefs.h"
#import "components/enterprise/connectors/core/features.h"
#import "components/policy/core/browser/policy_data_utils.h"
#import "components/policy/core/common/cloud/affiliation.h"
#import "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#import "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#import "components/policy/core/common/policy_types.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/enterprise/common/util.h"
#import "ios/chrome/browser/enterprise/connectors/connectors_manager.h"
#import "ios/chrome/browser/enterprise/connectors/connectors_util.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/web/public/web_client.h"

namespace enterprise_connectors {

ConnectorsService::ConnectorsService(
    PrefService* pref_service,
    signin::IdentityManager* identity_manager,
    policy::UserCloudPolicyManager* user_cloud_policy_manager,
    const std::string& profile_name,
    const base::FilePath& profile_path,
    bool is_off_the_record)
    : ConnectorsServiceBase(
          std::make_unique<ConnectorsManager>(pref_service,
                                              GetServiceProviderConfig())),
      pref_service_(pref_service),
      identity_manager_(identity_manager),
      user_cloud_policy_manager_(user_cloud_policy_manager),
      profile_name_(profile_name),
      profile_path_(profile_path),
      is_off_the_record_(is_off_the_record) {
  CHECK(pref_service_);
}

ConnectorsService::~ConnectorsService() = default;

std::string ConnectorsService::GetManagementDomain() {
  if (!ConnectorsEnabled()) {
    return std::string();
  }

  std::optional<policy::PolicyScope> policy_scope = std::nullopt;

  // Check the scope of the Url Filtering policy.
  if (std::optional<DmToken> dm_token =
          GetDmToken(kEnterpriseRealTimeUrlCheckScope)) {
    policy_scope = dm_token.value().scope;
  }

  // Machine scope has precedence, only update the scope if the previous
  // policy is not already machine-scoped.
  if (policy_scope != policy::PolicyScope::POLICY_SCOPE_MACHINE) {
    if (std::optional<DmToken> dm_token =
            GetDmToken(kOnSecurityEventScopePref)) {
      policy_scope = dm_token.value().scope;
    }
  }

  return enterprise::GetManagementDomain(policy_scope, identity_manager_);
}

std::optional<std::string> ConnectorsService::GetBrowserDmToken() const {
  return enterprise::GetBrowserDmToken();
}

std::optional<ConnectorsServiceBase::DmToken> ConnectorsService::GetDmToken(
    const char* scope_pref) const {
  policy::PolicyScope scope =
      static_cast<policy::PolicyScope>(pref_service_->GetInteger(scope_pref));
  if (scope == policy::PolicyScope::POLICY_SCOPE_USER) {
    auto profile_dm_token = GetProfileDmToken();
    if (profile_dm_token) {
      return DmToken(std::move(*profile_dm_token),
                     policy::PolicyScope::POLICY_SCOPE_USER);
    }
    return std::nullopt;
  }

  DCHECK_EQ(scope, policy::PolicyScope::POLICY_SCOPE_MACHINE);
  auto browser_dm_token = GetBrowserDmToken();
  if (!browser_dm_token) {
    return std::nullopt;
  }
  return DmToken(std::move(*browser_dm_token),
                 policy::PolicyScope::POLICY_SCOPE_MACHINE);
}

bool ConnectorsService::ConnectorsEnabled() const {
  return !is_off_the_record_;
}

PrefService* ConnectorsService::GetPrefs() {
  return pref_service_;
}

const PrefService* ConnectorsService::GetPrefs() const {
  return pref_service_;
}

policy::CloudPolicyManager*
ConnectorsService::GetManagedUserCloudPolicyManager() const {
  return user_cloud_policy_manager_;
}

std::unique_ptr<ClientMetadata> ConnectorsService::BuildClientMetadata(
    bool is_cloud) {
  auto reporting_settings = GetReportingSettings();
  if (is_cloud && !reporting_settings.has_value()) {
    return GetBasicClientMetadata();
  }

  auto metadata = std::make_unique<ClientMetadata>(GetContextAsClientMetadata(
      profile_name_, profile_path_, user_cloud_policy_manager_));
  if (!is_cloud) {
    PopulateBrowserMetadata(/*include_device_info=*/true,
                            metadata->mutable_browser());
  }
  metadata->set_is_chrome_os_managed_guest_session(false);
  bool include_device_info = IncludeDeviceInfo(
      reporting_settings.value().per_profile, IsProfileAffiliated());
  PopulateBrowserMetadata(include_device_info, metadata->mutable_browser());

  if (include_device_info) {
    PopulateDeviceMetadata(
        policy::BrowserDMTokenStorage::Get()->RetrieveClientId(),
        metadata->mutable_device());
  }
  return metadata;
}

std::unique_ptr<ClientMetadata> ConnectorsService::GetBasicClientMetadata() {
  auto metadata = std::make_unique<ClientMetadata>();
  // We need to return profile and browser DM tokens, even in cases where the
  // reporting policy is disabled, in order to support merging rules.
  std::optional<std::string> browser_dm_token = GetBrowserDmToken();
  if (browser_dm_token.has_value()) {
    metadata->mutable_device()->set_dm_token(*browser_dm_token);
  }

  std::optional<std::string> profile_dm_token =
      enterprise::GetUserDmToken(user_cloud_policy_manager_);
  if (profile_dm_token.has_value()) {
    metadata->mutable_profile()->set_dm_token(*profile_dm_token);
  }

  // This is to indicate the webProtect that the request is not coming from a
  // Managed Guest Session on ChromeOS
  metadata->set_is_chrome_os_managed_guest_session(false);
  return metadata;
}

bool ConnectorsService::IsProfileAffiliated() const {
  base::flat_set<std::string> user_ids;
  if (user_cloud_policy_manager_ && user_cloud_policy_manager_->core() &&
      user_cloud_policy_manager_->core()->store() &&
      user_cloud_policy_manager_->core()->store()->has_policy()) {
    const auto* policy_data =
        user_cloud_policy_manager_->core()->store()->policy();
    if (policy_data) {
      const auto& ids = policy_data->user_affiliation_ids();
      user_ids = {ids.begin(), ids.end()};
    }
  }

  base::flat_set<std::string> device_ids = GetApplicationContext()
                                               ->GetBrowserPolicyConnector()
                                               ->GetDeviceAffiliationIds();

  return policy::IsAffiliated(user_ids, device_ids);
}

std::string ConnectorsService::GetProfileEmail() const {
  return ::enterprise_connectors::GetProfileEmail(identity_manager_);
}

std::string ConnectorsService::GetDeviceClientId() const {
  return policy::BrowserDMTokenStorage::Get()->RetrieveClientId();
}

}  // namespace enterprise_connectors
