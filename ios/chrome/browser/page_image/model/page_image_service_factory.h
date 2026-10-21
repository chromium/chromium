// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAGE_IMAGE_MODEL_PAGE_IMAGE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PAGE_IMAGE_MODEL_PAGE_IMAGE_SERVICE_FACTORY_H_

#import <memory>

#import "components/page_image_service/image_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Factory for the components ImageService service which fetches salient images.
class PageImageServiceFactory final : public TypedProfileKeyedServiceFactoryIOS<
                                          PageImageServiceFactory,
                                          page_image_service::ImageService> {
 public:
  explicit PageImageServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_PAGE_IMAGE_MODEL_PAGE_IMAGE_SERVICE_FACTORY_H_
