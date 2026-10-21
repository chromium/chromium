// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_CLOUD_USER_POLICY_SIGNIN_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_CLOUD_USER_POLICY_SIGNIN_SERVICE_FACTORY_H_

#include "ios/chrome/browser/policy/model/cloud/user_policy_signin_service.h"
#include "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace policy {

// Singleton that owns all UserPolicySigninServices and creates/deletes them as
// new Profiles are created/shutdown.
//
// Warning: ONLY use the service when Enterprise Policy is enabled where
// the policy system objects are enabled (eg. the BrowserPolicyConnector object
// was instantiated).
class UserPolicySigninServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<UserPolicySigninServiceFactory,
                                                UserPolicySigninService> {
 public:
  explicit UserPolicySigninServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
  void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) final;
};

}  // namespace policy

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_CLOUD_USER_POLICY_SIGNIN_SERVICE_FACTORY_H_
