// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_RECONCILOR_FACTORY_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_RECONCILOR_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class AccountReconcilor;
class ChromeBrowserState;

namespace ios {
// Singleton that owns all AccountReconcilors and associates them with browser
// states.
class AccountReconcilorFactory : public BrowserStateKeyedServiceFactory {
 public:
  // Returns the instance of AccountReconcilor associated with this browser
  // state (creating one if none exists). Returns null if this browser state
  // cannot have an GaiaCookieManagerService (for example, if it is incognito).
  static AccountReconcilor* GetForBrowserState(
      ChromeBrowserState* browser_state);

  // Returns an instance of the factory singleton.
  static AccountReconcilorFactory* GetInstance();

  AccountReconcilorFactory(const AccountReconcilorFactory&) = delete;
  AccountReconcilorFactory& operator=(const AccountReconcilorFactory&) = delete;

 private:
  friend class base::NoDestructor<AccountReconcilorFactory>;

  AccountReconcilorFactory();
  ~AccountReconcilorFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_RECONCILOR_FACTORY_H_
