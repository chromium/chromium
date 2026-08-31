// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ON_DEVICE_CATEGORY_CLASSIFIER_IN_PROCESS_CATEGORY_CLASSIFICATION_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ON_DEVICE_CATEGORY_CLASSIFIER_IN_PROCESS_CATEGORY_CLASSIFICATION_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class InProcessCategoryClassificationService;
class ProfileIOS;

// Singleton that owns all InProcessCategoryClassificationServices and
// associates them with ProfileIOS.
class InProcessCategoryClassificationServiceFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static InProcessCategoryClassificationService* GetForProfile(
      ProfileIOS* profile);
  static InProcessCategoryClassificationServiceFactory* GetInstance();
  static TestingFactory GetDefaultFactory();

 private:
  friend class base::NoDestructor<
      InProcessCategoryClassificationServiceFactory>;

  InProcessCategoryClassificationServiceFactory();
  ~InProcessCategoryClassificationServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ON_DEVICE_CATEGORY_CLASSIFIER_IN_PROCESS_CATEGORY_CLASSIFICATION_SERVICE_FACTORY_H_
