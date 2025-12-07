// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cross_platform_promos/model/cross_platform_promos_service_factory.h"

#import "ios/chrome/browser/cross_platform_promos/model/cross_platform_promos_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
CrossPlatformPromosService* CrossPlatformPromosServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<CrossPlatformPromosService>(
      profile, true);
}

// static
CrossPlatformPromosServiceFactory*
CrossPlatformPromosServiceFactory::GetInstance() {
  static base::NoDestructor<CrossPlatformPromosServiceFactory> instance;
  return instance.get();
}

CrossPlatformPromosServiceFactory::CrossPlatformPromosServiceFactory()
    : ProfileKeyedServiceFactoryIOS("CrossPlatformPromosService") {}

CrossPlatformPromosServiceFactory::~CrossPlatformPromosServiceFactory() =
    default;

std::unique_ptr<KeyedService>
CrossPlatformPromosServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<CrossPlatformPromosService>(profile);
}
