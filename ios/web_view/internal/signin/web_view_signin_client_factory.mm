// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/signin/web_view_signin_client_factory.h"

#include "base/no_destructor.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/signin/public/base/signin_client.h"
#include "ios/web_view/internal/signin/ios_web_view_signin_client.h"
#include "ios/web_view/internal/web_view_browser_state.h"

namespace ios_web_view {

// static
IOSWebViewSigninClient* WebViewSigninClientFactory::GetForBrowserState(
    ios_web_view::WebViewBrowserState* browser_state) {
  return static_cast<IOSWebViewSigninClient*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
WebViewSigninClientFactory* WebViewSigninClientFactory::GetInstance() {
  static base::NoDestructor<WebViewSigninClientFactory> instance;
  return instance.get();
}

WebViewSigninClientFactory::WebViewSigninClientFactory()
    : BrowserStateKeyedServiceFactory(
          "SigninClient",
          BrowserStateDependencyManager::GetInstance()) {
}

std::unique_ptr<KeyedService>
WebViewSigninClientFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  WebViewBrowserState* browser_state =
      WebViewBrowserState::FromBrowserState(context);
  return std::make_unique<IOSWebViewSigninClient>(browser_state->GetPrefs(),
                                                  browser_state);
}

}  // namespace ios_web_view
