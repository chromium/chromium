// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_IMAGE_FETCHER_MODEL_IMAGE_FETCHER_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_IMAGE_FETCHER_MODEL_IMAGE_FETCHER_SERVICE_FACTORY_H_

#import "components/image_fetcher/core/image_fetcher_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns all ImageFetcherService and associates them with
// profile.
class ImageFetcherServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          ImageFetcherServiceFactory,
          image_fetcher::ImageFetcherService> {
 public:
  explicit ImageFetcherServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_IMAGE_FETCHER_MODEL_IMAGE_FETCHER_SERVICE_FACTORY_H_
