// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_AUTHENTICATION_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_AUTHENTICATION_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace user_prefs {
class PrefRegistrySyncable;
}

class AuthenticationService;
class AuthenticationServiceDelegate;

// Singleton that owns all `AuthenticationServices` and associates them with
// profiles. Listens for the profile's destruction notification and cleans up
// the associated `AuthenticationService`.
class AuthenticationServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  // Factory for AuthenticationServiceDelegate.
  using AuthenticationServiceDelegateFactory =
      base::OnceCallback<std::unique_ptr<AuthenticationServiceDelegate>(
          ProfileIOS*)>;

  static AuthenticationService* GetForProfile(ProfileIOS* profile);
  static AuthenticationServiceFactory* GetInstance();

  // Returns a factory that builds an AuthenticationService using a custom
  // delegate instance (needs to be constructible before the profile).
  static TestingFactory GetFactoryWithDelegate(
      std::unique_ptr<AuthenticationServiceDelegate> delegate);

  // Returns a factory that builds an AuthenticationService using a custom
  // delegate factory.
  static TestingFactory GetFactoryWithDelegateFactory(
      AuthenticationServiceDelegateFactory delegate_factory);

 private:
  friend class base::NoDestructor<AuthenticationServiceFactory>;

  AuthenticationServiceFactory();
  ~AuthenticationServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  void RegisterBrowserStatePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_AUTHENTICATION_SERVICE_FACTORY_H_
