// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LANGUAGE_MODEL_URL_LANGUAGE_HISTOGRAM_FACTORY_H_
#define IOS_CHROME_BROWSER_LANGUAGE_MODEL_URL_LANGUAGE_HISTOGRAM_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;

namespace language {
class UrlLanguageHistogram;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

class UrlLanguageHistogramFactory : public BrowserStateKeyedServiceFactory {
 public:
  static UrlLanguageHistogramFactory* GetInstance();
  static language::UrlLanguageHistogram* GetForBrowserState(
      ChromeBrowserState* browser_state);

  UrlLanguageHistogramFactory(const UrlLanguageHistogramFactory&) = delete;
  UrlLanguageHistogramFactory& operator=(const UrlLanguageHistogramFactory&) =
      delete;

 private:
  friend class base::NoDestructor<UrlLanguageHistogramFactory>;

  UrlLanguageHistogramFactory();
  ~UrlLanguageHistogramFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  void RegisterBrowserStatePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

#endif  // IOS_CHROME_BROWSER_LANGUAGE_MODEL_URL_LANGUAGE_HISTOGRAM_FACTORY_H_
