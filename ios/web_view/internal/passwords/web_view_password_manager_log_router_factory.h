// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_PASSWORDS_WEB_VIEW_PASSWORD_MANAGER_LOG_ROUTER_FACTORY_H_
#define IOS_WEB_VIEW_INTERNAL_PASSWORDS_WEB_VIEW_PASSWORD_MANAGER_LOG_ROUTER_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace autofill {
class LogRouter;
}

namespace ios_web_view {
class WebViewBrowserState;

// Singleton that owns all PasswordStores and associates them with
// WebViewBrowserState.
class WebViewPasswordManagerLogRouterFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static autofill::LogRouter* GetForBrowserState(
      WebViewBrowserState* browser_state);

  static WebViewPasswordManagerLogRouterFactory* GetInstance();

  WebViewPasswordManagerLogRouterFactory(
      const WebViewPasswordManagerLogRouterFactory&) = delete;
  WebViewPasswordManagerLogRouterFactory& operator=(
      const WebViewPasswordManagerLogRouterFactory&) = delete;

 private:
  friend class base::NoDestructor<WebViewPasswordManagerLogRouterFactory>;

  WebViewPasswordManagerLogRouterFactory();
  ~WebViewPasswordManagerLogRouterFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_PASSWORDS_WEB_VIEW_PASSWORD_MANAGER_LOG_ROUTER_FACTORY_H_
