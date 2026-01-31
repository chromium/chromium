// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/model/ntp_background_image_cache_service_factory.h"

#import "base/no_destructor.h"
#import "ios/chrome/browser/home_customization/model/home_background_customization_service_factory.h"
#import "ios/chrome/browser/ntp/model/ntp_background_image_cache_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
NTPBackgroundImageCacheService*
NTPBackgroundImageCacheServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<NTPBackgroundImageCacheService>(
      profile, /*create=*/true);
}

// static
NTPBackgroundImageCacheServiceFactory*
NTPBackgroundImageCacheServiceFactory::GetInstance() {
  static base::NoDestructor<NTPBackgroundImageCacheServiceFactory> instance;
  return instance.get();
}

NTPBackgroundImageCacheServiceFactory::NTPBackgroundImageCacheServiceFactory()
    : ProfileKeyedServiceFactoryIOS("NTPBackgroundImageCacheService") {
  DependsOn(HomeBackgroundCustomizationServiceFactory::GetInstance());
}

NTPBackgroundImageCacheServiceFactory::
    ~NTPBackgroundImageCacheServiceFactory() {}

std::unique_ptr<KeyedService>
NTPBackgroundImageCacheServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<NTPBackgroundImageCacheService>(
      HomeBackgroundCustomizationServiceFactory::GetForProfile(profile));
}
