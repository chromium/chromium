// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_IMAGE_FETCHER_MODEL_IMAGE_FETCHER_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_IMAGE_FETCHER_MODEL_IMAGE_FETCHER_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "components/image_fetcher/core/image_fetcher_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ImageFetcherService;

// Singleton that owns all ImageFetcherService and associates them with
// profile.
class ImageFetcherServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static image_fetcher::ImageFetcherService* GetForProfile(ProfileIOS* profile);
  static ImageFetcherServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<ImageFetcherServiceFactory>;

  ImageFetcherServiceFactory();
  ~ImageFetcherServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_IMAGE_FETCHER_MODEL_IMAGE_FETCHER_SERVICE_FACTORY_H_
