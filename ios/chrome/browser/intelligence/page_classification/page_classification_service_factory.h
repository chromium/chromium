// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_CLASSIFICATION_PAGE_CLASSIFICATION_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_CLASSIFICATION_PAGE_CLASSIFICATION_SERVICE_FACTORY_H_

#import <memory>

#import "ios/chrome/browser/intelligence/page_classification/page_classification_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns all PageClassificationService instances and associates
// them with Profiles.
class PageClassificationServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          PageClassificationServiceFactory,
          PageClassificationService> {
 public:
  explicit PageClassificationServiceFactory(PassKey key);

  static TestingFactory GetDefaultFactory();

 private:
  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_CLASSIFICATION_PAGE_CLASSIFICATION_SERVICE_FACTORY_H_
