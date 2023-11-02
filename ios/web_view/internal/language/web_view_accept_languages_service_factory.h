// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_LANGUAGE_WEB_VIEW_ACCEPT_LANGUAGES_SERVICE_FACTORY_H_
#define IOS_WEB_VIEW_INTERNAL_LANGUAGE_WEB_VIEW_ACCEPT_LANGUAGES_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace language {
class AcceptLanguagesService;
}

namespace ios_web_view {

class WebViewBrowserState;

// WebViewAcceptLanguagesServiceFactory is a way to associate an
// AcceptLanguagesService instance to a BrowserState.
class WebViewAcceptLanguagesServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static language::AcceptLanguagesService* GetForBrowserState(
      WebViewBrowserState* browser_state);
  static WebViewAcceptLanguagesServiceFactory* GetInstance();

  WebViewAcceptLanguagesServiceFactory(
      const WebViewAcceptLanguagesServiceFactory&) = delete;
  WebViewAcceptLanguagesServiceFactory& operator=(
      const WebViewAcceptLanguagesServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<WebViewAcceptLanguagesServiceFactory>;

  WebViewAcceptLanguagesServiceFactory();
  ~WebViewAcceptLanguagesServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_LANGUAGE_WEB_VIEW_ACCEPT_LANGUAGES_SERVICE_FACTORY_H_
