// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ON_DEVICE_CATEGORY_CLASSIFIER_IN_PROCESS_CATEGORY_CLASSIFICATION_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ON_DEVICE_CATEGORY_CLASSIFIER_IN_PROCESS_CATEGORY_CLASSIFICATION_SERVICE_FACTORY_H_

#import <memory>

#import "ios/chrome/browser/intelligence/on_device_category_classifier/in_process_category_classification_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns all InProcessCategoryClassificationServices and
// associates them with ProfileIOS.
class InProcessCategoryClassificationServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          InProcessCategoryClassificationServiceFactory,
          InProcessCategoryClassificationService> {
 public:
  explicit InProcessCategoryClassificationServiceFactory(PassKey key);
  static TestingFactory GetDefaultFactory();

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ON_DEVICE_CATEGORY_CLASSIFIER_IN_PROCESS_CATEGORY_CLASSIFICATION_SERVICE_FACTORY_H_
