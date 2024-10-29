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

  // Force the instantiation of AuthenticationService and initialize it with
  // the given delegate. Must be called before GetForProfile (not doing
  // so is a security issue and the app will terminate).
  // DEPRECATED: install a factory returned by GetFactoryWithDelegate()
  // or GetFactoryWithDelegateFactory() instead of calling this method.
  static void CreateAndInitializeForProfile(
      ProfileIOS* profile,
      std::unique_ptr<AuthenticationServiceDelegate> delegate);

  // DEPRECATED: install a factory returned by GetFactoryWithDelegate()
  // or GetFactoryWithDelegateFactory() instead of calling this method.
  static void CreateAndInitializeForBrowserState(
      ProfileIOS* profile,
      std::unique_ptr<AuthenticationServiceDelegate> delegate);

  // Returns the default factory used to build AuthenticationServices. Can be
  // registered with SetTestingFactory to use real instances during testing.
  static TestingFactory GetDefaultFactory();

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
