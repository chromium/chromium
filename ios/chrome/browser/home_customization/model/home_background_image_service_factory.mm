// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/model/home_background_image_service_factory.h"

#import "base/no_destructor.h"
#import "ios/chrome/browser/home_customization/model/home_background_image_service.h"
#import "ios/chrome/browser/home_customization/model/ntp_background_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
HomeBackgroundImageService* HomeBackgroundImageServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<HomeBackgroundImageService>(
      profile, /*create=*/true);
}

// static
HomeBackgroundImageServiceFactory*
HomeBackgroundImageServiceFactory::GetInstance() {
  static base::NoDestructor<HomeBackgroundImageServiceFactory> instance;
  return instance.get();
}

HomeBackgroundImageServiceFactory::HomeBackgroundImageServiceFactory()
    : ProfileKeyedServiceFactoryIOS("HomeBackgroundImageService") {
  DependsOn(NtpBackgroundServiceFactory::GetInstance());
}

HomeBackgroundImageServiceFactory::~HomeBackgroundImageServiceFactory() {}

std::unique_ptr<KeyedService>
HomeBackgroundImageServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<HomeBackgroundImageService>(
      NtpBackgroundServiceFactory::GetForProfile(profile));
}
