// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_AUTHENTICATION_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_AUTHENTICATION_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

class AuthenticationService;
class AuthenticationServiceDelegate;

// Singleton that owns all `AuthenticationServices` and associates them with
// profiles. Listens for the `BrowserState`'s destruction notification and
// cleans up the associated `AuthenticationService`.
class AuthenticationServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  // TODO(crbug.com/358301380): remove this method.
  static AuthenticationService* GetForBrowserState(ProfileIOS* profile);

  static AuthenticationService* GetForProfile(ProfileIOS* profile);
  static AuthenticationServiceFactory* GetInstance();

  // Force the instantiation of AuthenticationService and initialize it with
  // the given delegate. Must be called before GetForProfile (not doing
  // so is a security issue and the app will terminate).
  static void CreateAndInitializeForProfile(
      ProfileIOS* profile,
      std::unique_ptr<AuthenticationServiceDelegate> delegate);

  // Deprecated, use CreateAndInitializeForProfile() instead.
  static void CreateAndInitializeForBrowserState(
      ProfileIOS* profile,
      std::unique_ptr<AuthenticationServiceDelegate> delegate);

  // Returns the default factory used to build AuthenticationServices. Can be
  // registered with SetTestingFactory to use real instances during testing.
  static TestingFactory GetDefaultFactory();

  AuthenticationServiceFactory(const AuthenticationServiceFactory&) = delete;
  AuthenticationServiceFactory& operator=(const AuthenticationServiceFactory&) =
      delete;

 private:
  friend class base::NoDestructor<AuthenticationServiceFactory>;

  AuthenticationServiceFactory();
  ~AuthenticationServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  void RegisterBrowserStatePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;

  // KeyedServiceBaseFactory implementation.
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_AUTHENTICATION_SERVICE_FACTORY_H_
