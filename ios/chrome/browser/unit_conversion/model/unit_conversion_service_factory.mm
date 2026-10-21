// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/unit_conversion/model/unit_conversion_service_factory.h"

#import <utility>

#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/unit_conversion/model/unit_conversion_service.h"

UnitConversionServiceFactory::UnitConversionServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "UnitConversionService",
          ProfileSelection::kOwnInstanceInIncognito) {}

std::unique_ptr<KeyedService>
UnitConversionServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<UnitConversionService>();
}
