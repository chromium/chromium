// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_AUTHENTICATION_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_AUTHENTICATION_SERVICE_FACTORY_H_

#import <memory>

#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

// Singleton that owns all `AuthenticationServices` and associates them with
// profiles. Listens for the profile's destruction notification and cleans up
// the associated `AuthenticationService`.
class AuthenticationServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<AuthenticationServiceFactory,
                                                AuthenticationService> {
 public:
  explicit AuthenticationServiceFactory(PassKey key);

  // Returns the default factory, useful in tests.
  static TestingFactory GetDefaultFactory();

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
  void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) final;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_AUTHENTICATION_SERVICE_FACTORY_H_
