// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_IDENTITY_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_SIGNIN_IDENTITY_MANAGER_FACTORY_H_

#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class IdentityManagerFactoryObserver;

namespace signin {
class IdentityManager;
}

namespace ios {
class ChromeBrowserState;
}

// Singleton that owns all IdentityManager instances and associates them with
// BrowserStates.
class IdentityManagerFactory : public BrowserStateKeyedServiceFactory {
 public:
  static signin::IdentityManager* GetForBrowserState(
      ios::ChromeBrowserState* browser_state);
  static signin::IdentityManager* GetForBrowserStateIfExists(
      ios::ChromeBrowserState* browser_state);

  // Returns an instance of the IdentityManagerFactory singleton.
  static IdentityManagerFactory* GetInstance();

  // Methods to register or remove observers of IdentityManager
  // creation/shutdown.
  void AddObserver(IdentityManagerFactoryObserver* observer);
  void RemoveObserver(IdentityManagerFactoryObserver* observer);

 private:
  friend class base::NoDestructor<IdentityManagerFactory>;

  IdentityManagerFactory();
  ~IdentityManagerFactory() override;

  // List of observers. Checks that list is empty on destruction.
  base::ObserverList<IdentityManagerFactoryObserver,
                     /*check_empty=*/true,
                     /*allow_reentrancy=*/false>
      observer_list_;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  void BrowserStateShutdown(web::BrowserState* context) override;
  void RegisterBrowserStatePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;

  DISALLOW_COPY_AND_ASSIGN(IdentityManagerFactory);
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_IDENTITY_MANAGER_FACTORY_H_
