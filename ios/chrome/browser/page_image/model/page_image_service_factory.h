// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAGE_IMAGE_MODEL_PAGE_IMAGE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PAGE_IMAGE_MODEL_PAGE_IMAGE_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

namespace page_image_service {
class ImageService;
}  // namespace page_image_service

// Factory for the components ImageService service which fetches salient images.
class PageImageServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  // TODO(crbug.com/358301380): remove this method.
  static page_image_service::ImageService* GetForBrowserState(
      ProfileIOS* profile);

  static page_image_service::ImageService* GetForProfile(ProfileIOS* profile);
  static PageImageServiceFactory* GetInstance();

  PageImageServiceFactory(const PageImageServiceFactory&) = delete;
  PageImageServiceFactory& operator=(const PageImageServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<PageImageServiceFactory>;

  PageImageServiceFactory();
  ~PageImageServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_PAGE_IMAGE_MODEL_PAGE_IMAGE_SERVICE_FACTORY_H_
