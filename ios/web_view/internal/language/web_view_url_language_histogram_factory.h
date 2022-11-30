// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_LANGUAGE_WEB_VIEW_URL_LANGUAGE_HISTOGRAM_FACTORY_H_
#define IOS_WEB_VIEW_INTERNAL_LANGUAGE_WEB_VIEW_URL_LANGUAGE_HISTOGRAM_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace language {
class UrlLanguageHistogram;
}  // namespace language

namespace ios_web_view {

class WebViewBrowserState;

class WebViewUrlLanguageHistogramFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static language::UrlLanguageHistogram* GetForBrowserState(
      WebViewBrowserState* browser_state);
  static WebViewUrlLanguageHistogramFactory* GetInstance();

  WebViewUrlLanguageHistogramFactory(
      const WebViewUrlLanguageHistogramFactory&) = delete;
  WebViewUrlLanguageHistogramFactory& operator=(
      const WebViewUrlLanguageHistogramFactory&) = delete;

 private:
  friend class base::NoDestructor<WebViewUrlLanguageHistogramFactory>;

  WebViewUrlLanguageHistogramFactory();
  ~WebViewUrlLanguageHistogramFactory() override = default;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  void RegisterBrowserStatePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_LANGUAGE_WEB_VIEW_URL_LANGUAGE_HISTOGRAM_FACTORY_H_
