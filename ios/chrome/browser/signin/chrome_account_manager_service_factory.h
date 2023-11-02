// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_CHROME_ACCOUNT_MANAGER_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SIGNIN_CHROME_ACCOUNT_MANAGER_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;
class ChromeAccountManagerService;

// Singleton that owns all ChromeAccountManagerServices and associates them with
// ChromeBrowserState.
class ChromeAccountManagerServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  ChromeAccountManagerServiceFactory(const BrowserStateKeyedServiceFactory&) =
      delete;
  ChromeAccountManagerServiceFactory& operator=(
      const BrowserStateKeyedServiceFactory&) = delete;

  static ChromeAccountManagerService* GetForBrowserState(
      ChromeBrowserState* browser_state);
  static ChromeAccountManagerServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<ChromeAccountManagerServiceFactory>;

  ChromeAccountManagerServiceFactory();
  ~ChromeAccountManagerServiceFactory() override;

  // ChromeAccountManagerServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_CHROME_ACCOUNT_MANAGER_SERVICE_FACTORY_H_
