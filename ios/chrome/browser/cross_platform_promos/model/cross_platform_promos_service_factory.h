// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CROSS_PLATFORM_PROMOS_MODEL_CROSS_PLATFORM_PROMOS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_CROSS_PLATFORM_PROMOS_MODEL_CROSS_PLATFORM_PROMOS_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class CrossPlatformPromosService;
class ProfileIOS;

// Singleton that owns all CrossPlatformPromosServices and associates them with
// Profiles.
class CrossPlatformPromosServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static CrossPlatformPromosService* GetForProfile(ProfileIOS* profile);
  static CrossPlatformPromosServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<CrossPlatformPromosServiceFactory>;

  CrossPlatformPromosServiceFactory();
  ~CrossPlatformPromosServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_CROSS_PLATFORM_PROMOS_MODEL_CROSS_PLATFORM_PROMOS_SERVICE_FACTORY_H_
