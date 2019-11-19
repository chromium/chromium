// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_MANAGER_LOG_ROUTER_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_MANAGER_LOG_ROUTER_FACTORY_H_

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace autofill {
class LogRouter;
}

namespace ios {
class ChromeBrowserState;

// Singleton that owns all PasswordStores and associates them with
// ios::ChromeBrowserState.
class PasswordManagerLogRouterFactory : public BrowserStateKeyedServiceFactory {
 public:
  static autofill::LogRouter* GetForBrowserState(
      ios::ChromeBrowserState* browser_state);

  static PasswordManagerLogRouterFactory* GetInstance();

 private:
  friend class base::NoDestructor<PasswordManagerLogRouterFactory>;

  PasswordManagerLogRouterFactory();
  ~PasswordManagerLogRouterFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;

  DISALLOW_COPY_AND_ASSIGN(PasswordManagerLogRouterFactory);
};

}  // namespace ios
#endif  // IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_MANAGER_LOG_ROUTER_FACTORY_H_
