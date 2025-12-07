// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_HOME_BACKGROUND_IMAGE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_HOME_BACKGROUND_IMAGE_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class HomeBackgroundImageService;

// Singleton that owns all HomeBackgroundImageServices and associates them with
// profile.
class HomeBackgroundImageServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static HomeBackgroundImageService* GetForProfile(ProfileIOS* profile);
  static HomeBackgroundImageServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<HomeBackgroundImageServiceFactory>;

  HomeBackgroundImageServiceFactory();
  ~HomeBackgroundImageServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_HOME_BACKGROUND_IMAGE_SERVICE_FACTORY_H_
