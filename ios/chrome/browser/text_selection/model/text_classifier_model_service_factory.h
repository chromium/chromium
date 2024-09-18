// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TEXT_SELECTION_MODEL_TEXT_CLASSIFIER_MODEL_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_TEXT_SELECTION_MODEL_TEXT_CLASSIFIER_MODEL_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class TextClassifierModelService;

// Singleton that owns all TextClassifierModelService(s) and associates them
// with ProfileIOS.
class TextClassifierModelServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  // TODO(crbug.com/358301380): remove this method.
  static TextClassifierModelService* GetForBrowserState(ProfileIOS* profile);

  static TextClassifierModelService* GetForProfile(ProfileIOS* profile);
  static TextClassifierModelServiceFactory* GetInstance();

  TextClassifierModelServiceFactory(const TextClassifierModelServiceFactory&) =
      delete;
  TextClassifierModelServiceFactory& operator=(
      const TextClassifierModelServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<TextClassifierModelServiceFactory>;

  TextClassifierModelServiceFactory();
  ~TextClassifierModelServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
  bool ServiceIsCreatedWithBrowserState() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // IOS_CHROME_BROWSER_TEXT_SELECTION_MODEL_TEXT_CLASSIFIER_MODEL_SERVICE_FACTORY_H_
