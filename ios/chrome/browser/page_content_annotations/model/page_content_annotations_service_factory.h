// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_MODEL_PAGE_CONTENT_ANNOTATIONS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_MODEL_PAGE_CONTENT_ANNOTATIONS_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

namespace page_content_annotations {
class PageContentAnnotationsService;
}  // namespace page_content_annotations

// Singleton that owns all PageContentAnnotationsService(s) and associates them
// with ProfileIOS. No service is created for incognito profile.
class PageContentAnnotationsServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static page_content_annotations::PageContentAnnotationsService* GetForProfile(
      ProfileIOS* profile);
  static PageContentAnnotationsServiceFactory* GetInstance();

  // Returns the default factory used to build PageContentAnnotationsService.
  // Can be registered with SetTestingFactory to use real instances during
  // testing.
  static TestingFactory GetDefaultFactory();

 private:
  friend class base::NoDestructor<PageContentAnnotationsServiceFactory>;

  PageContentAnnotationsServiceFactory();
  ~PageContentAnnotationsServiceFactory() override;
  PageContentAnnotationsServiceFactory(
      const PageContentAnnotationsServiceFactory&) = delete;
  PageContentAnnotationsServiceFactory& operator=(
      const PageContentAnnotationsServiceFactory&) = delete;

  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  bool ServiceIsCreatedWithBrowserState() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // IOS_CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_MODEL_PAGE_CONTENT_ANNOTATIONS_SERVICE_FACTORY_H_
