// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_ACCOUNT_CONSISTENCY_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SIGNIN_ACCOUNT_CONSISTENCY_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class AccountConsistencyService;
class ChromeBrowserState;

namespace ios {
// Singleton that creates the AccountConsistencyService(s) and associates those
// services  with browser states.
class AccountConsistencyServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  // Returns the instance of AccountConsistencyService associated with this
  // browser state (creating one if none exists). Returns null if this browser
  // state cannot have an AccountConsistencyService (for example, if it is
  // incognito or if WKWebView is not enabled).
  static AccountConsistencyService* GetForBrowserState(
      ChromeBrowserState* browser_state);

  // Returns an instance of the factory singleton.
  static AccountConsistencyServiceFactory* GetInstance();

  AccountConsistencyServiceFactory(const AccountConsistencyServiceFactory&) =
      delete;
  AccountConsistencyServiceFactory& operator=(
      const AccountConsistencyServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<AccountConsistencyServiceFactory>;

  AccountConsistencyServiceFactory();
  ~AccountConsistencyServiceFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_SIGNIN_ACCOUNT_CONSISTENCY_SERVICE_FACTORY_H_
