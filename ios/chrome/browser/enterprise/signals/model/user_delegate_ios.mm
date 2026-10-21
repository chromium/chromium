// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/signals/model/user_delegate_ios.h"

#import <utility>

#import "base/check.h"
#import "components/enterprise/device_trust/core/common_types.h"
#import "components/policy/core/common/cloud/affiliation.h"
#import "components/policy/core/common/policy_types.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/policy/model/profile_policy_connector.h"

namespace {

// Converts `policy_level` into its corresponding `policy::PolicyScope`.
policy::PolicyScope GetPolicyScope(
    enterprise_connectors::DTCPolicyLevel policy_level) {
  switch (policy_level) {
    case enterprise_connectors::DTCPolicyLevel::kUser:
      return policy::POLICY_SCOPE_USER;
    case enterprise_connectors::DTCPolicyLevel::kBrowser:
      return policy::POLICY_SCOPE_MACHINE;
  }
}

}  // namespace

UserDelegateIOS::UserDelegateIOS(
    ProfilePolicyConnector* profile_policy_connector,
    DeviceAffiliationIdsCallback device_affiliation_ids_callback,
    signin::IdentityManager* identity_manager,
    device_signals::UserDelegate::SignalsDependencyDelegate*
        device_trust_connector_service)
    : profile_policy_connector_(profile_policy_connector),
      device_affiliation_ids_callback_(
          std::move(device_affiliation_ids_callback)),
      identity_manager_(identity_manager),
      device_trust_connector_service_(device_trust_connector_service) {
  CHECK(device_affiliation_ids_callback_);
}

UserDelegateIOS::~UserDelegateIOS() = default;

bool UserDelegateIOS::IsAffiliated() const {
  if (!profile_policy_connector_) {
    return false;
  }
  return policy::IsAffiliated(
      profile_policy_connector_->GetUserAffiliationIds(),
      device_affiliation_ids_callback_.Run());
}

bool UserDelegateIOS::IsManagedUser() const {
  return profile_policy_connector_ && profile_policy_connector_->IsManaged();
}

bool UserDelegateIOS::IsSameUser(const GaiaId& gaia_id) const {
  return identity_manager_ &&
         identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
                 .gaia == gaia_id;
}

std::set<policy::PolicyScope> UserDelegateIOS::GetPolicyScopesNeedingSignals()
    const {
  std::set<policy::PolicyScope> policy_scopes;
  if (device_trust_connector_service_) {
    for (const enterprise_connectors::DTCPolicyLevel policy_level :
         device_trust_connector_service_->GetSignalsPolicyScope()) {
      policy_scopes.insert(GetPolicyScope(policy_level));
    }
  }
  return policy_scopes;
}

bool UserDelegateIOS::IsCollectSignalsConsentRequired() const {
  return true;
}
