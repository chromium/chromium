// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PHOTOS_MODEL_PHOTOS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PHOTOS_MODEL_PHOTOS_SERVICE_FACTORY_H_

#import <memory>

#import "ios/chrome/browser/photos/model/photos_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns all PhotosService-s and associates them with
// Profile.
class PhotosServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<PhotosServiceFactory,
                                                PhotosService> {
 public:
  explicit PhotosServiceFactory(PassKey key);

  // Returns the default factory.
  static TestingFactory GetDefaultFactory();

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_PHOTOS_MODEL_PHOTOS_SERVICE_FACTORY_H_
