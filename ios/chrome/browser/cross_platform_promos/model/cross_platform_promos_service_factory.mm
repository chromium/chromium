// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cross_platform_promos/model/cross_platform_promos_service_factory.h"

#import <utility>

#import "ios/chrome/browser/cross_platform_promos/model/cross_platform_promos_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

CrossPlatformPromosServiceFactory::CrossPlatformPromosServiceFactory(
    PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(std::move(key),
                                         "CrossPlatformPromosService") {}

std::unique_ptr<KeyedService>
CrossPlatformPromosServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<CrossPlatformPromosService>(profile);
}
