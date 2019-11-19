// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_AUTOFILL_WEB_VIEW_AUTOFILL_LOG_ROUTER_FACTORY_H_
#define IOS_WEB_VIEW_INTERNAL_AUTOFILL_WEB_VIEW_AUTOFILL_LOG_ROUTER_FACTORY_H_

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace ios_web_view {
class WebViewBrowserState;
}  // namespace ios_web_view

namespace autofill {

class LogRouter;

// A factory that associates autofill::LogRouter instances with
// web::BrowserState. This returns nullptr of off-the-record browser states.
class WebViewAutofillLogRouterFactory : public BrowserStateKeyedServiceFactory {
 public:
  static autofill::LogRouter* GetForBrowserState(
      ios_web_view::WebViewBrowserState* browser_state);

  static WebViewAutofillLogRouterFactory* GetInstance();

 private:
  friend class base::NoDestructor<WebViewAutofillLogRouterFactory>;

  WebViewAutofillLogRouterFactory();
  ~WebViewAutofillLogRouterFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;

  DISALLOW_COPY_AND_ASSIGN(WebViewAutofillLogRouterFactory);
};

}  // namespace autofill

#endif  // IOS_WEB_VIEW_INTERNAL_AUTOFILL_WEB_VIEW_AUTOFILL_LOG_ROUTER_FACTORY_H_
