// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_IDENTITY_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_IDENTITY_MANAGER_FACTORY_H_

#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns all IdentityManager instances and associates them with
// Profiles.
class IdentityManagerFactory final
    : public TypedProfileKeyedServiceFactoryIOS<IdentityManagerFactory,
                                                signin::IdentityManager> {
 public:
  explicit IdentityManagerFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
  void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) final;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_IDENTITY_MANAGER_FACTORY_H_
