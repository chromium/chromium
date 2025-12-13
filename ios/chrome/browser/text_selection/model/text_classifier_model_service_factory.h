// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TEXT_SELECTION_MODEL_TEXT_CLASSIFIER_MODEL_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_TEXT_SELECTION_MODEL_TEXT_CLASSIFIER_MODEL_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class TextClassifierModelService;

// Singleton that owns all TextClassifierModelService(s) and associates them
// with ProfileIOS.
class TextClassifierModelServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static TextClassifierModelService* GetForProfile(ProfileIOS* profile);
  static TextClassifierModelServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<TextClassifierModelServiceFactory>;

  TextClassifierModelServiceFactory();
  ~TextClassifierModelServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_TEXT_SELECTION_MODEL_TEXT_CLASSIFIER_MODEL_SERVICE_FACTORY_H_
