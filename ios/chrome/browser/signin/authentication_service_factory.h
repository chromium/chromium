// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_AUTHENTICATION_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SIGNIN_AUTHENTICATION_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

class AuthenticationService;
class AuthenticationServiceDelegate;
class ChromeBrowserState;

// Singleton that owns all `AuthenticationServices` and associates them with
// browser states. Listens for the `BrowserState`'s destruction notification and
// cleans up the associated `AuthenticationService`.
class AuthenticationServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static AuthenticationService* GetForBrowserState(
      ChromeBrowserState* browser_state);
  static AuthenticationServiceFactory* GetInstance();

  // Force the instantiation of AuthenticationService and initialize it with
  // the given delegate. Must be called before GetForBrowserState (not doing
  // so is a security issue and the app will terminate).
  static void CreateAndInitializeForBrowserState(
      ChromeBrowserState* browser_state,
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

#endif  // IOS_CHROME_BROWSER_SIGNIN_AUTHENTICATION_SERVICE_FACTORY_H_
