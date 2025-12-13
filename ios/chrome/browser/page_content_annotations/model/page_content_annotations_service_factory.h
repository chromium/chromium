// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_MODEL_PAGE_CONTENT_ANNOTATIONS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_MODEL_PAGE_CONTENT_ANNOTATIONS_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace page_content_annotations {
class PageContentAnnotationsService;
}  // namespace page_content_annotations

// Singleton that owns all PageContentAnnotationsService(s) and associates them
// with ProfileIOS. No service is created for incognito profile.
class PageContentAnnotationsServiceFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static page_content_annotations::PageContentAnnotationsService* GetForProfile(
      ProfileIOS* profile);
  static PageContentAnnotationsServiceFactory* GetInstance();

  // Returns the default factory used to build PageContentAnnotationsService.
  // Can be registered with AddTestingFactory to use real instances during
  // testing.
  static TestingFactory GetDefaultFactory();

 private:
  friend class base::NoDestructor<PageContentAnnotationsServiceFactory>;

  PageContentAnnotationsServiceFactory();
  ~PageContentAnnotationsServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_MODEL_PAGE_CONTENT_ANNOTATIONS_SERVICE_FACTORY_H_
