// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_IDENTITY_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_IDENTITY_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;
class IdentityManagerFactoryObserver;

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
  void RegisterBrowserStatePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_IDENTITY_MANAGER_FACTORY_H_
