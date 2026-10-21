// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_MODEL_PAGE_CONTENT_ANNOTATIONS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_MODEL_PAGE_CONTENT_ANNOTATIONS_SERVICE_FACTORY_H_

#import <memory>

#import "components/page_content_annotations/core/page_content_annotations_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns all PageContentAnnotationsService(s) and associates them
// with ProfileIOS. No service is created for incognito profile.
class PageContentAnnotationsServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          PageContentAnnotationsServiceFactory,
          page_content_annotations::PageContentAnnotationsService> {
 public:
  explicit PageContentAnnotationsServiceFactory(PassKey key);

  // Returns the default factory used to build PageContentAnnotationsService.
  // Can be registered with AddTestingFactory to use real instances during
  // testing.
  static TestingFactory GetDefaultFactory();

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_MODEL_PAGE_CONTENT_ANNOTATIONS_SERVICE_FACTORY_H_
