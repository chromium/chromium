// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_CLOUD_USER_POLICY_SIGNIN_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_CLOUD_USER_POLICY_SIGNIN_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace policy {

class DeviceManagementService;
class UserPolicySigninService;

// Singleton that owns all UserPolicySigninServices and creates/deletes them as
// new Profiles are created/shutdown.
//
// Warning: ONLY use the service when Enterprise Policy is enabled where
// the policy system objects are enabled (eg. the BrowserPolicyConnector object
// was instantiated).
class UserPolicySigninServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  // Returns an instance of the UserPolicySigninServiceFactory singleton.
  static UserPolicySigninServiceFactory* GetInstance();

  // Returns the instance of UserPolicySigninService for the `context`.
  static UserPolicySigninService* GetForProfile(ProfileIOS* profile);

  // Allows setting a mock DeviceManagementService for tests. Does not take
  // ownership, and should be reset to nullptr at the end of the test.
  // Set this before an instance is built for a Profile.
  static void SetDeviceManagementServiceForTesting(
      DeviceManagementService* device_management_service);

  UserPolicySigninServiceFactory(const UserPolicySigninServiceFactory&) =
      delete;
  UserPolicySigninServiceFactory& operator=(
      const UserPolicySigninServiceFactory&) = delete;

 protected:
  // BrowserStateKeyedServiceFactory implementation:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* browser_state) const override;
  void RegisterBrowserStatePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;

 private:
  bool ServiceIsCreatedWithBrowserState() const override;
  bool ServiceIsNULLWhileTesting() const override;

  friend struct base::DefaultSingletonTraits<UserPolicySigninServiceFactory>;

  UserPolicySigninServiceFactory();
  ~UserPolicySigninServiceFactory() override;
};

}  // namespace policy

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_CLOUD_USER_POLICY_SIGNIN_SERVICE_FACTORY_H_
