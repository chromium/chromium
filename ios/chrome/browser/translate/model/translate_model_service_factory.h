// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TRANSLATE_MODEL_TRANSLATE_MODEL_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_TRANSLATE_MODEL_TRANSLATE_MODEL_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;

namespace translate {
class TranslateModelService;
}

class TranslateModelServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static translate::TranslateModelService* GetForBrowserState(
      ChromeBrowserState* browser_state);
  static TranslateModelServiceFactory* GetInstance();

  TranslateModelServiceFactory(const TranslateModelServiceFactory&) = delete;
  TranslateModelServiceFactory& operator=(const TranslateModelServiceFactory&) =
      delete;

 private:
  friend class base::NoDestructor<TranslateModelServiceFactory>;

  TranslateModelServiceFactory();
  ~TranslateModelServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_TRANSLATE_MODEL_TRANSLATE_MODEL_SERVICE_FACTORY_H_
