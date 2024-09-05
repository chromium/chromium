// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_IDENTITY_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_IDENTITY_MANAGER_FACTORY_H_

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

namespace signin {
class IdentityManager;
}

// Singleton that owns all IdentityManager instances and associates them with
// BrowserStates.
class IdentityManagerFactory : public BrowserStateKeyedServiceFactory {
 public:
  static signin::IdentityManager* GetForBrowserState(
      ChromeBrowserState* browser_state);
  static signin::IdentityManager* GetForBrowserStateIfExists(
      ChromeBrowserState* browser_state);

  // Returns an instance of the IdentityManagerFactory singleton.
  static IdentityManagerFactory* GetInstance();

  IdentityManagerFactory(const IdentityManagerFactory&) = delete;
  IdentityManagerFactory& operator=(const IdentityManagerFactory&) = delete;

 private:
  friend class base::NoDestructor<IdentityManagerFactory>;

  IdentityManagerFactory();
  ~IdentityManagerFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  void RegisterBrowserStatePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_IDENTITY_MANAGER_FACTORY_H_
