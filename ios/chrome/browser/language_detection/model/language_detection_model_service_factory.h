// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LANGUAGE_DETECTION_MODEL_LANGUAGE_DETECTION_MODEL_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_LANGUAGE_DETECTION_MODEL_LANGUAGE_DETECTION_MODEL_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

namespace language_detection {
class LanguageDetectionModelService;
}

class LanguageDetectionModelServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static language_detection::LanguageDetectionModelService* GetForProfile(
      ProfileIOS* profile);
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

#endif  // IOS_CHROME_BROWSER_LANGUAGE_DETECTION_MODEL_LANGUAGE_DETECTION_MODEL_SERVICE_FACTORY_H_
