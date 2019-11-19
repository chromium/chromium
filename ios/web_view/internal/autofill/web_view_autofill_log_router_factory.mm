// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/web_view_autofill_log_router_factory.h"

#include <memory>
#include <utility>

#include "base/no_destructor.h"
#include "components/autofill/core/browser/logging/log_router.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/web_view/internal/web_view_browser_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace autofill {

// static
LogRouter* WebViewAutofillLogRouterFactory::GetForBrowserState(
    ios_web_view::WebViewBrowserState* browser_state) {
  return static_cast<LogRouter*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
WebViewAutofillLogRouterFactory*
WebViewAutofillLogRouterFactory::GetInstance() {
  static base::NoDestructor<WebViewAutofillLogRouterFactory> instance;
  return instance.get();
}

WebViewAutofillLogRouterFactory::WebViewAutofillLogRouterFactory()
    : BrowserStateKeyedServiceFactory(
          "AutofillInternalsService",
          BrowserStateDependencyManager::GetInstance()) {}

WebViewAutofillLogRouterFactory::~WebViewAutofillLogRouterFactory() {}

std::unique_ptr<KeyedService>
WebViewAutofillLogRouterFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return std::make_unique<LogRouter>();
}

}  // namespace autofill
