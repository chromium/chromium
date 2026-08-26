// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ON_DEVICE_CATEGORY_CLASSIFIER_ON_DEVICE_PAGE_CLASSIFICATION_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ON_DEVICE_CATEGORY_CLASSIFIER_ON_DEVICE_PAGE_CLASSIFICATION_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class OnDevicePageClassificationService;
class ProfileIOS;

// Singleton that owns all OnDevicePageClassificationServices and associates
// them with ProfileIOS.
class OnDevicePageClassificationServiceFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static OnDevicePageClassificationService* GetForProfile(ProfileIOS* profile);
  static OnDevicePageClassificationServiceFactory* GetInstance();
  static TestingFactory GetDefaultFactory();

 private:
  friend class base::NoDestructor<OnDevicePageClassificationServiceFactory>;

  OnDevicePageClassificationServiceFactory();
  ~OnDevicePageClassificationServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ON_DEVICE_CATEGORY_CLASSIFIER_ON_DEVICE_PAGE_CLASSIFICATION_SERVICE_FACTORY_H_
