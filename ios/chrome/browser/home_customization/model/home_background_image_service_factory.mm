// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/model/home_background_image_service_factory.h"

#import <utility>

#import "ios/chrome/browser/home_customization/model/home_background_image_service.h"
#import "ios/chrome/browser/home_customization/model/ntp_background_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

HomeBackgroundImageServiceFactory::HomeBackgroundImageServiceFactory(
    PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(std::move(key),
                                         "HomeBackgroundImageService") {
  DependsOn(NtpBackgroundServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
HomeBackgroundImageServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<HomeBackgroundImageService>(
      NtpBackgroundServiceFactory::GetForProfile(profile));
}
