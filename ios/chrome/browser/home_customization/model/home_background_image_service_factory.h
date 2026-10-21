// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_HOME_BACKGROUND_IMAGE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_HOME_BACKGROUND_IMAGE_SERVICE_FACTORY_H_

#import "ios/chrome/browser/home_customization/model/home_background_image_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns all HomeBackgroundImageServices and associates them with
// profile.
class HomeBackgroundImageServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          HomeBackgroundImageServiceFactory,
          HomeBackgroundImageService> {
 public:
  explicit HomeBackgroundImageServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_HOME_BACKGROUND_IMAGE_SERVICE_FACTORY_H_
