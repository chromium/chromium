// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/ui_bundled/management_util.h"

#import "base/strings/string_split.h"
#import "components/account_id/account_id.h"
#import "components/policy/core/browser/webui/policy_data_utils.h"
#import "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#import "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/account_managed_status_finder.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "google_apis/gaia/gaia_auth_util.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/policy/ui_bundled/user_policy_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"

namespace {

// Gets the AccountId from the provided `account_info`.
AccountId AccountIdFromAccountInfo(const CoreAccountInfo& account_info) {
  if (account_info.email.empty() || account_info.gaia.empty()) {
    return EmptyAccountId();
  }

  return AccountId::FromUserEmailGaiaId(
      gaia::CanonicalizeEmail(account_info.email), account_info.gaia);
}

// Extracts the domain from the email. Returns std::nullopt if there is no
// domain.
std::optional<std::string> ExtractDomainFromEmail(const std::string& email) {
  std::vector<std::string> components = base::SplitString(
      email, "@", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (components.size() != 2) {
    return std::nullopt;
  }
  const std::string domain = components[1];
  if (domain.empty()) {
    return std::nullopt;
  }

  return domain;
}

std::optional<std::string> GetMachineLevelPolicyDomain() {
  policy::MachineLevelUserCloudPolicyManager* manager =
      GetApplicationContext()
          ->GetBrowserPolicyConnector()
          ->machine_level_user_cloud_policy_manager();
  return policy::GetManagedBy(manager);
}

std::optional<std::string> GetUserPolicyDomain(
    signin::IdentityManager* identity_manager,
    AuthenticationService* auth_service,
    PrefService* prefs) {
  if (!CanFetchUserPolicy(auth_service, prefs)) {
    return std::nullopt;
  }

  AccountId account_id = AccountIdFromAccountInfo(
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));

  const std::string user_email = account_id.GetUserEmail();

  if (user_email.empty()) {
    return std::nullopt;
  }

  if (!signin::AccountManagedStatusFinder::MayBeEnterpriseUserBasedOnEmail(
          user_email)) {
    return std::nullopt;
  }

  return ExtractDomainFromEmail(user_email);
}

}  // namespace

ManagementState GetManagementState(signin::IdentityManager* identity_manager,
                                   AuthenticationService* auth_service,
                                   PrefService* prefs) {
  ManagementState management_state;
  management_state.machine_level_domain = GetMachineLevelPolicyDomain();
  management_state.user_level_domain =
      GetUserPolicyDomain(identity_manager, auth_service, prefs);

  BrowserPolicyConnectorIOS* policy_connector =
      GetApplicationContext()->GetBrowserPolicyConnector();
  management_state.has_machine_level_policy =
      policy_connector && policy_connector->HasMachineLevelPolicies();

  return management_state;
}
