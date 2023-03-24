// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROMOS_MANAGER_PROMOS_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_PROMOS_MANAGER_PROMOS_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;
class PromosManager;

// Singleton that owns all PromosManagers and associates them with
// ChromeBrowserState.
class PromosManagerFactory : public BrowserStateKeyedServiceFactory {
 public:
  static PromosManager* GetForBrowserState(ChromeBrowserState* browser_state);

  static PromosManagerFactory* GetInstance();

  PromosManagerFactory(const PromosManagerFactory&) = delete;
  PromosManagerFactory& operator=(const PromosManagerFactory&) = delete;

 private:
  friend class base::NoDestructor<PromosManagerFactory>;

  PromosManagerFactory();
  ~PromosManagerFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* state) const override;
};

#endif  // IOS_CHROME_BROWSER_PROMOS_MANAGER_PROMOS_MANAGER_FACTORY_H_
