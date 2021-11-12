// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TRANSLATE_TRANSLATE_ACCEPT_LANGUAGES_FACTORY_H_
#define IOS_CHROME_BROWSER_TRANSLATE_TRANSLATE_ACCEPT_LANGUAGES_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;

namespace translate {
class TranslateAcceptLanguages;
}

// TranslateAcceptLanguagesFactory is a way to associate a
// TranslateAcceptLanguages instance to a BrowserState.
class TranslateAcceptLanguagesFactory : public BrowserStateKeyedServiceFactory {
 public:
  static translate::TranslateAcceptLanguages* GetForBrowserState(
      ChromeBrowserState* browser_state);
  static TranslateAcceptLanguagesFactory* GetInstance();

  TranslateAcceptLanguagesFactory(const TranslateAcceptLanguagesFactory&) =
      delete;
  TranslateAcceptLanguagesFactory& operator=(
      const TranslateAcceptLanguagesFactory&) = delete;

 private:
  friend class base::NoDestructor<TranslateAcceptLanguagesFactory>;

  TranslateAcceptLanguagesFactory();
  ~TranslateAcceptLanguagesFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_TRANSLATE_TRANSLATE_ACCEPT_LANGUAGES_FACTORY_H_
