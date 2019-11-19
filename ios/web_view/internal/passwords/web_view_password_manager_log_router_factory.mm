// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/passwords/web_view_password_manager_log_router_factory.h"

#include <memory>
#include <utility>

#include "base/no_destructor.h"
#include "components/autofill/core/browser/logging/log_router.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/web_view/internal/web_view_browser_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

// static
autofill::LogRouter* WebViewPasswordManagerLogRouterFactory::GetForBrowserState(
    WebViewBrowserState* browser_state) {
  return static_cast<autofill::LogRouter*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
WebViewPasswordManagerLogRouterFactory*
WebViewPasswordManagerLogRouterFactory::GetInstance() {
  static base::NoDestructor<WebViewPasswordManagerLogRouterFactory> instance;
  return instance.get();
}

WebViewPasswordManagerLogRouterFactory::WebViewPasswordManagerLogRouterFactory()
    : BrowserStateKeyedServiceFactory(
          "PasswordManagerInternalsService",
          BrowserStateDependencyManager::GetInstance()) {}

WebViewPasswordManagerLogRouterFactory::
    ~WebViewPasswordManagerLogRouterFactory() {}

std::unique_ptr<KeyedService>
WebViewPasswordManagerLogRouterFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return std::make_unique<autofill::LogRouter>();
}

}  // namespace ios_web_view
