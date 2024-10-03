// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/connectors_service.h"

#import "components/enterprise/browser/controller/browser_dm_token_storage.h"
#import "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#import "components/policy/core/common/policy_types.h"

namespace enterprise_connectors {

ConnectorsService::ConnectorsService(
    bool off_the_record,
    PrefService* pref_service,
    policy::UserCloudPolicyManager* user_cloud_policy_manager)
    : off_the_record_(off_the_record),
      prefs_(pref_service),
      user_cloud_policy_manager_(user_cloud_policy_manager) {
  DCHECK(prefs_);
}

bool ConnectorsService::IsConnectorEnabled(AnalysisConnector connector) const {
  // None of the analysis connector policies are supported on iOS.
  return false;
}

std::optional<ConnectorsServiceBase::DmToken> ConnectorsService::GetDmToken(
    const char* scope_pref) const {
  policy::PolicyScope scope =
      static_cast<policy::PolicyScope>(prefs_->GetInteger(scope_pref));
  if (scope == policy::PolicyScope::POLICY_SCOPE_USER) {
    auto profile_dm_token = GetProfileDmToken();
    if (profile_dm_token) {
      return DmToken(std::move(*profile_dm_token),
                     policy::PolicyScope::POLICY_SCOPE_USER);
    }
    return std::nullopt;
  }

  DCHECK_EQ(scope, policy::PolicyScope::POLICY_SCOPE_MACHINE);
  auto browser_dm_token =
      policy::BrowserDMTokenStorage::Get()->RetrieveDMToken();
  if (!browser_dm_token.is_valid()) {
    return std::nullopt;
  }

  return DmToken(browser_dm_token.value(),
                 policy::PolicyScope::POLICY_SCOPE_MACHINE);
}

bool ConnectorsService::ConnectorsEnabled() const {
  return !off_the_record_;
}

PrefService* ConnectorsService::GetPrefs() {
  return prefs_;
}

const PrefService* ConnectorsService::GetPrefs() const {
  return prefs_;
}

ConnectorsManagerBase* ConnectorsService::GetConnectorsManagerBase() {
  // TODO(crbug.com/370466578): Implement this method.
  return nullptr;
}

const ConnectorsManagerBase* ConnectorsService::GetConnectorsManagerBase()
    const {
  // TODO(crbug.com/370466578): Implement this method.
  return nullptr;
}

policy::CloudPolicyManager*
ConnectorsService::GetManagedUserCloudPolicyManager() const {
  return user_cloud_policy_manager_.get();
}

}  // namespace enterprise_connectors
