// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_CLOUD_USER_POLICY_SIGNIN_SERVICE_H_
#define IOS_CHROME_BROWSER_POLICY_CLOUD_USER_POLICY_SIGNIN_SERVICE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/policy/core/browser/cloud/user_policy_signin_service_base.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class ChromeBrowserState;

namespace policy {

class CloudPolicyClientRegistrationHelper;

// A specialization of UserPolicySigninServiceBase for iOS.
class UserPolicySigninService : public UserPolicySigninServiceBase,
                                public signin::IdentityManager::Observer {
 public:
  // Creates a UserPolicySigninService associated with the `browser_state`.
  UserPolicySigninService(
      PrefService* browser_state_prefs,
      PrefService* local_state,
      DeviceManagementService* device_management_service,
      UserCloudPolicyManager* policy_manager,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory);
  UserPolicySigninService(const UserPolicySigninService&) = delete;
  UserPolicySigninService& operator=(const UserPolicySigninService&) = delete;
  ~UserPolicySigninService() override;

  // signin::IdentityManager::Observer implementation:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;

  // KeyedService implementation:
  void Shutdown() override;

  // Notifies that the user has seen the notification about User Policy.
  void OnUserPolicyNotificationSeen();

 private:
  // UserPolicySigninServiceBase implementation:
  base::TimeDelta GetTryRegistrationDelay() override;
  void ProhibitSignoutIfNeeded() override;
  void UpdateLastPolicyCheckTime() override;
  signin::ConsentLevel GetConsentLevelForRegistration() override;
  bool CanApplyPolicies(bool check_for_refresh_token) override;

  // Tries to initialize the service if a signed in account is available and
  // eligible for user policy.
  void TryInitialize();

  // Helper used to register for user policy.
  std::unique_ptr<CloudPolicyClientRegistrationHelper> registration_helper_;

  // The PrefService associated with the BrowserState.
  raw_ptr<PrefService> browser_state_prefs_;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      scoped_identity_manager_observation_{this};
};

}  // namespace policy

#endif  // IOS_CHROME_BROWSER_POLICY_CLOUD_USER_POLICY_SIGNIN_SERVICE_H_
