// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TRANSLATE_LANGUAGE_DETECTION_MODEL_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_TRANSLATE_LANGUAGE_DETECTION_MODEL_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;

namespace translate {
class LanguageDetectionModelService;
}  // namespace translate

// This is a workaround for crbug/1324530 on iOS where it is mandatory to have
// LanguageDetectionModel scoped by BrowserState.
// TODO(crbug.com/1324530): remove this class once TranslateModelService does
// this.
class LanguageDetectionModelServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static translate::LanguageDetectionModelService* GetForBrowserState(
      ChromeBrowserState* browser_state);
  static LanguageDetectionModelServiceFactory* GetInstance();

  LanguageDetectionModelServiceFactory(
      const LanguageDetectionModelServiceFactory&) = delete;
  LanguageDetectionModelServiceFactory& operator=(
      const LanguageDetectionModelServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<LanguageDetectionModelServiceFactory>;

  LanguageDetectionModelServiceFactory();
  ~LanguageDetectionModelServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_TRANSLATE_LANGUAGE_DETECTION_MODEL_SERVICE_FACTORY_H_
