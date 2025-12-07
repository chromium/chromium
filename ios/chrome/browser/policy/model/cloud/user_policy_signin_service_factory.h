// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_CLOUD_USER_POLICY_SIGNIN_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_CLOUD_USER_POLICY_SIGNIN_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace policy {

class UserPolicySigninService;

// Singleton that owns all UserPolicySigninServices and creates/deletes them as
// new Profiles are created/shutdown.
//
// Warning: ONLY use the service when Enterprise Policy is enabled where
// the policy system objects are enabled (eg. the BrowserPolicyConnector object
// was instantiated).
class UserPolicySigninServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  // Returns an instance of the UserPolicySigninServiceFactory singleton.
  static UserPolicySigninServiceFactory* GetInstance();

  // Returns the instance of UserPolicySigninService for the `context`.
  static UserPolicySigninService* GetForProfile(ProfileIOS* profile);

 protected:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;

 private:
  friend struct base::DefaultSingletonTraits<UserPolicySigninServiceFactory>;

  UserPolicySigninServiceFactory();
  ~UserPolicySigninServiceFactory() override;
};

}  // namespace policy

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_CLOUD_USER_POLICY_SIGNIN_SERVICE_FACTORY_H_
