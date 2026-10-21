// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_SIGNALS_MODEL_USER_DELEGATE_IOS_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_SIGNALS_MODEL_USER_DELEGATE_IOS_H_

#import <set>
#import <string>

#import "base/containers/flat_set.h"
#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "components/device_signals/core/browser/user_delegate.h"

class ProfilePolicyConnector;

namespace signin {
class IdentityManager;
}  // namespace signin

class UserDelegateIOS : public device_signals::UserDelegate {
 public:
  // Callback that returns the set of device affiliation IDs.
  using DeviceAffiliationIdsCallback =
      base::RepeatingCallback<base::flat_set<std::string>()>;

  UserDelegateIOS(ProfilePolicyConnector* profile_policy_connector,
                  DeviceAffiliationIdsCallback device_affiliation_ids_callback,
                  signin::IdentityManager* identity_manager,
                  device_signals::UserDelegate::SignalsDependencyDelegate*
                      device_trust_connector_service);
  ~UserDelegateIOS() override;

  UserDelegateIOS(const UserDelegateIOS&) = delete;
  UserDelegateIOS& operator=(const UserDelegateIOS&) = delete;

  // UserDelegate:
  bool IsAffiliated() const override;
  bool IsManagedUser() const override;
  bool IsSameUser(const GaiaId& gaia_id) const override;
  std::set<policy::PolicyScope> GetPolicyScopesNeedingSignals() const override;
  bool IsCollectSignalsConsentRequired() const override;

 private:
  const raw_ptr<ProfilePolicyConnector> profile_policy_connector_;
  const DeviceAffiliationIdsCallback device_affiliation_ids_callback_;
  const raw_ptr<signin::IdentityManager> identity_manager_;
  const raw_ptr<device_signals::UserDelegate::SignalsDependencyDelegate>
      device_trust_connector_service_;
};

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_SIGNALS_MODEL_USER_DELEGATE_IOS_H_
