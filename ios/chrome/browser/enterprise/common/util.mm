// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/common/util.h"

#import "components/enterprise/browser/controller/browser_dm_token_storage.h"
#import "components/enterprise/connectors/core/common.h"
#import "components/policy/core/browser/policy_data_utils.h"
#import "components/policy/core/common/cloud/cloud_policy_core.h"
#import "components/policy/core/common/cloud/cloud_policy_store.h"
#import "components/policy/core/common/cloud/dm_token.h"
#import "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#import "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "google_apis/gaia/gaia_auth_util.h"
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

namespace enterprise {

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

std::optional<std::string> GetBrowserDmToken() {
  auto browser_dm_token =
      policy::BrowserDMTokenStorage::Get()->RetrieveDMToken();
  if (!browser_dm_token.is_valid()) {
    return std::nullopt;
  }

  return browser_dm_token.value();
}

std::string GetManagementDomain(std::optional<policy::PolicyScope> policy_scope,
                                ProfileIOS* profile) {
  // Return empty string if:
  // a. None of the policies are enabled.
  // b. Profile is null.
  if (!policy_scope || !profile) {
    return std::string();
  }

  switch (*policy_scope) {
      // Retrieve the domain via profile email for user-scoped policies.
    case policy::PolicyScope::POLICY_SCOPE_USER: {
      std::string profile_email = enterprise_connectors::GetProfileEmail(
          IdentityManagerFactory::GetForProfile(profile));
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

}  // namespace enterprise
