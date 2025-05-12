// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/connectors_service.h"

#import "base/feature_list.h"
#import "base/types/expected.h"
#import "components/enterprise/browser/controller/browser_dm_token_storage.h"
#import "components/enterprise/connectors/core/common.h"
#import "components/enterprise/connectors/core/connectors_prefs.h"
#import "components/enterprise/connectors/core/features.h"
#import "components/policy/core/browser/policy_data_utils.h"
#import "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#import "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#import "components/policy/core/common/policy_types.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "google_apis/gaia/gaia_auth_util.h"
#import "ios/chrome/browser/enterprise/connectors/connectors_util.h"
#import "ios/chrome/browser/enterprise/connectors/features.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

namespace {
std::string GetDomainFromEmail(const std::string& email) {
  size_t email_separator_pos = email.find('@');
  if (email.empty() || email_separator_pos == std::string::npos ||
      email_separator_pos == email.size() - 1) {
    return std::string();
  }
  return gaia::ExtractDomainName(email);
}

}  // namespace

namespace enterprise_connectors {

ConnectorsService::ConnectorsService(ProfileIOS* profile) : profile_(profile) {
  CHECK(profile_);
  identity_manager_ = IdentityManagerFactory::GetForProfile(profile);

  CHECK(profile_->IsOffTheRecord() || identity_manager_ != nullptr);
  connectors_manager_ = std::make_unique<ConnectorsManager>(
      profile_->GetPrefs(), GetServiceProviderConfig());
}

ConnectorsService::~ConnectorsService() = default;

std::string ConnectorsService::GetManagementDomain() {
  if (!ConnectorsEnabled()) {
    return std::string();
  }

  std::optional<policy::PolicyScope> policy_scope = std::nullopt;

  // Check the scope of the Url Filtering policy.
  if (base::FeatureList::IsEnabled(kIOSEnterpriseRealtimeUrlFiltering)) {
    if (std::optional<DmToken> dm_token =
            GetDmToken(kEnterpriseRealTimeUrlCheckScope)) {
      policy_scope = dm_token.value().scope;
    }
  }

  // Check the scope of Event Reporting policy.
  if (base::FeatureList::IsEnabled(kEnterpriseRealtimeEventReportingOnIOS)) {
    // Machine scope has precedence, only update the scope if the previous
    // policy is not already machine-scoped.
    if (policy_scope != policy::PolicyScope::POLICY_SCOPE_MACHINE) {
      if (std::optional<DmToken> dm_token =
              GetDmToken(kOnSecurityEventScopePref)) {
        policy_scope = dm_token.value().scope;
      }
    }
  }

  // Return empty string if none of the policies are enabled.
  if (!policy_scope) {
    return std::string();
  }

  // Retrieve the management domain for the give scope.
  switch (*policy_scope) {
      // Retrieve the domain via profile email for user-scoped policies.
    case policy::PolicyScope::POLICY_SCOPE_USER: {
      std::string profile_email = GetProfileEmail(identity_manager_);
      return GetDomainFromEmail(profile_email);
    }
    case policy::PolicyScope::POLICY_SCOPE_MACHINE:
      policy::MachineLevelUserCloudPolicyManager* manager =
          GetApplicationContext()
              ->GetBrowserPolicyConnector()
              ->machine_level_user_cloud_policy_manager();
      return policy::GetManagedBy(manager).value_or(std::string());
  }
}

bool ConnectorsService::IsConnectorEnabled(AnalysisConnector connector) const {
  // None of the analysis connector policies are supported on iOS.
  return false;
}

std::optional<std::string> ConnectorsService::GetBrowserDmToken() const {
  auto browser_dm_token =
      policy::BrowserDMTokenStorage::Get()->RetrieveDMToken();
  if (!browser_dm_token.is_valid()) {
    return std::nullopt;
  }

  return browser_dm_token.value();
}

std::optional<ConnectorsServiceBase::DmToken> ConnectorsService::GetDmToken(
    const char* scope_pref) const {
  policy::PolicyScope scope = static_cast<policy::PolicyScope>(
      profile_->GetPrefs()->GetInteger(scope_pref));
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
  return !profile_->IsOffTheRecord();
}

PrefService* ConnectorsService::GetPrefs() {
  return profile_->GetPrefs();
}

const PrefService* ConnectorsService::GetPrefs() const {
  return profile_->GetPrefs();
}

ConnectorsManagerBase* ConnectorsService::GetConnectorsManagerBase() {
  return connectors_manager_.get();
}

const ConnectorsManagerBase* ConnectorsService::GetConnectorsManagerBase()
    const {
  return connectors_manager_.get();
}

policy::CloudPolicyManager*
ConnectorsService::GetManagedUserCloudPolicyManager() const {
  return profile_->GetUserCloudPolicyManager();
}

std::unique_ptr<ClientMetadata> ConnectorsService::BuildClientMetadata(
    bool is_cloud) {
  auto reporting_settings = GetReportingSettings();
  if (is_cloud && !reporting_settings.has_value()) {
    return GetBasicClientMetadata();
  }

  auto metadata =
      std::make_unique<ClientMetadata>(GetContextAsClientMetadata(profile_));
  if (!is_cloud) {
    PopulateBrowserMetadata(/*include_device_info=*/true,
                            metadata->mutable_browser());
  }
  metadata->set_is_chrome_os_managed_guest_session(false);
  bool include_device_info =
      IncludeDeviceInfo(profile_, reporting_settings.value().per_profile);
  PopulateBrowserMetadata(include_device_info, metadata->mutable_browser());

  if (include_device_info) {
    PopulateDeviceMetadata(
        reporting_settings.value(),
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

  std::optional<std::string> profile_dm_token = GetUserDmToken(profile_);
  if (profile_dm_token.has_value()) {
    metadata->mutable_profile()->set_dm_token(*profile_dm_token);
  }

  // This is to indicate the webProtect that the request is not coming from a
  // Managed Guest Session on ChromeOS
  metadata->set_is_chrome_os_managed_guest_session(false);
  return metadata;
}

}  // namespace enterprise_connectors
