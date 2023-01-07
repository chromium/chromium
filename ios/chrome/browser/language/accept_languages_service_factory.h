// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LANGUAGE_ACCEPT_LANGUAGES_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_LANGUAGE_ACCEPT_LANGUAGES_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;

namespace language {
class AcceptLanguagesService;
}

// AcceptLanguagesServiceFactory is a way to associate an
// AcceptLanguagesService instance to a BrowserState.
class AcceptLanguagesServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static language::AcceptLanguagesService* GetForBrowserState(
      ChromeBrowserState* browser_state);
  static AcceptLanguagesServiceFactory* GetInstance();

  AcceptLanguagesServiceFactory(const AcceptLanguagesServiceFactory&) = delete;
  AcceptLanguagesServiceFactory& operator=(
      const AcceptLanguagesServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<AcceptLanguagesServiceFactory>;

  AcceptLanguagesServiceFactory();
  ~AcceptLanguagesServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_LANGUAGE_ACCEPT_LANGUAGES_SERVICE_FACTORY_H_
