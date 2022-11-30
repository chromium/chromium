// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_SIGNIN_WEB_VIEW_SIGNIN_CLIENT_FACTORY_H_
#define IOS_WEB_VIEW_INTERNAL_SIGNIN_WEB_VIEW_SIGNIN_CLIENT_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class IOSWebViewSigninClient;

namespace ios_web_view {
class WebViewBrowserState;

// Singleton that owns all SigninClients and associates them with
// a browser state.
class WebViewSigninClientFactory : public BrowserStateKeyedServiceFactory {
 public:
  static IOSWebViewSigninClient* GetForBrowserState(
      ios_web_view::WebViewBrowserState* browser_state);
  static WebViewSigninClientFactory* GetInstance();

  WebViewSigninClientFactory(const WebViewSigninClientFactory&) = delete;
  WebViewSigninClientFactory& operator=(const WebViewSigninClientFactory&) =
      delete;

 private:
  friend class base::NoDestructor<WebViewSigninClientFactory>;

  WebViewSigninClientFactory();
  ~WebViewSigninClientFactory() override = default;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_SIGNIN_WEB_VIEW_SIGNIN_CLIENT_FACTORY_H_
