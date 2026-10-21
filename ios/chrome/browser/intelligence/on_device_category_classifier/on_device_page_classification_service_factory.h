// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ON_DEVICE_CATEGORY_CLASSIFIER_ON_DEVICE_PAGE_CLASSIFICATION_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ON_DEVICE_CATEGORY_CLASSIFIER_ON_DEVICE_PAGE_CLASSIFICATION_SERVICE_FACTORY_H_

#import <memory>

#import "ios/chrome/browser/intelligence/on_device_category_classifier/on_device_page_classification_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns all OnDevicePageClassificationServices and associates
// them with ProfileIOS.
class OnDevicePageClassificationServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          OnDevicePageClassificationServiceFactory,
          OnDevicePageClassificationService> {
 public:
  explicit OnDevicePageClassificationServiceFactory(PassKey key);
  static TestingFactory GetDefaultFactory();

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ON_DEVICE_CATEGORY_CLASSIFIER_ON_DEVICE_PAGE_CLASSIFICATION_SERVICE_FACTORY_H_
