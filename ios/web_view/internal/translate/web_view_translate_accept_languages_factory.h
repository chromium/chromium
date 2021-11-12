// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_TRANSLATE_WEB_VIEW_TRANSLATE_ACCEPT_LANGUAGES_FACTORY_H_
#define IOS_WEB_VIEW_INTERNAL_TRANSLATE_WEB_VIEW_TRANSLATE_ACCEPT_LANGUAGES_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace translate {
class TranslateAcceptLanguages;
}

namespace ios_web_view {

class WebViewBrowserState;

// TranslateAcceptLanguagesFactory is a way to associate a
// TranslateAcceptLanguages instance to a BrowserState.
class WebViewTranslateAcceptLanguagesFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static translate::TranslateAcceptLanguages* GetForBrowserState(
      WebViewBrowserState* browser_state);
  static WebViewTranslateAcceptLanguagesFactory* GetInstance();

  WebViewTranslateAcceptLanguagesFactory(
      const WebViewTranslateAcceptLanguagesFactory&) = delete;
  WebViewTranslateAcceptLanguagesFactory& operator=(
      const WebViewTranslateAcceptLanguagesFactory&) = delete;

 private:
  friend class base::NoDestructor<WebViewTranslateAcceptLanguagesFactory>;

  WebViewTranslateAcceptLanguagesFactory();
  ~WebViewTranslateAcceptLanguagesFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_TRANSLATE_WEB_VIEW_TRANSLATE_ACCEPT_LANGUAGES_FACTORY_H_
