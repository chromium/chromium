// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TEXT_SELECTION_MODEL_TEXT_CLASSIFIER_MODEL_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_TEXT_SELECTION_MODEL_TEXT_CLASSIFIER_MODEL_SERVICE_FACTORY_H_

#import <memory>

#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"
#import "ios/chrome/browser/text_selection/model/text_classifier_model_service.h"

// Singleton that owns all TextClassifierModelService(s) and associates them
// with ProfileIOS.
class TextClassifierModelServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          TextClassifierModelServiceFactory,
          TextClassifierModelService> {
 public:
  explicit TextClassifierModelServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_TEXT_SELECTION_MODEL_TEXT_CLASSIFIER_MODEL_SERVICE_FACTORY_H_
