// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_PASSWORD_MANAGER_LOG_ROUTER_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_PASSWORD_MANAGER_LOG_ROUTER_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;

namespace autofill {
class LogRouter;
}

namespace ios {
// Singleton that owns all PasswordStores and associates them with
// ChromeBrowserState.
class PasswordManagerLogRouterFactory : public BrowserStateKeyedServiceFactory {
 public:
  static autofill::LogRouter* GetForBrowserState(
      ChromeBrowserState* browser_state);

  static PasswordManagerLogRouterFactory* GetInstance();

  PasswordManagerLogRouterFactory(const PasswordManagerLogRouterFactory&) =
      delete;
  PasswordManagerLogRouterFactory& operator=(
      const PasswordManagerLogRouterFactory&) = delete;

 private:
  friend class base::NoDestructor<PasswordManagerLogRouterFactory>;

  PasswordManagerLogRouterFactory();
  ~PasswordManagerLogRouterFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

}  // namespace ios
#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_PASSWORD_MANAGER_LOG_ROUTER_FACTORY_H_
