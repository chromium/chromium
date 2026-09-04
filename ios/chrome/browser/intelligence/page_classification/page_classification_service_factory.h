// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_CLASSIFICATION_PAGE_CLASSIFICATION_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_CLASSIFICATION_PAGE_CLASSIFICATION_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class PageClassificationService;
class ProfileIOS;

// Singleton that owns all PageClassificationService instances and associates
// them with Profiles.
class PageClassificationServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static PageClassificationService* GetForProfile(ProfileIOS* profile);
  static PageClassificationServiceFactory* GetInstance();

  static ProfileKeyedServiceFactoryIOS::TestingFactory GetDefaultFactory();

 private:
  friend class base::NoDestructor<PageClassificationServiceFactory>;

  PageClassificationServiceFactory();
  ~PageClassificationServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_CLASSIFICATION_PAGE_CLASSIFICATION_SERVICE_FACTORY_H_
