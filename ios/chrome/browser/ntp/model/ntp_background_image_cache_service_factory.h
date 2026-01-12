// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_MODEL_NTP_BACKGROUND_IMAGE_CACHE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_NTP_MODEL_NTP_BACKGROUND_IMAGE_CACHE_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class NTPBackgroundImageCacheService;

// Singleton that owns all NTPBackgroundImageCacheServices and associates them
// with profiles.
class NTPBackgroundImageCacheServiceFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static NTPBackgroundImageCacheService* GetForProfile(ProfileIOS* profile);
  static NTPBackgroundImageCacheServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<NTPBackgroundImageCacheServiceFactory>;

  NTPBackgroundImageCacheServiceFactory();
  ~NTPBackgroundImageCacheServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_NTP_MODEL_NTP_BACKGROUND_IMAGE_CACHE_SERVICE_FACTORY_H_
