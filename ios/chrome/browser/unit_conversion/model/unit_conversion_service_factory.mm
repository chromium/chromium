// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/unit_conversion/model/unit_conversion_service_factory.h"

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/unit_conversion/model/unit_conversion_service.h"

// static
UnitConversionServiceFactory* UnitConversionServiceFactory::GetInstance() {
  static base::NoDestructor<UnitConversionServiceFactory> instance;
  return instance.get();
}

// static
UnitConversionService* UnitConversionServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<UnitConversionService>(
      profile, /*create=*/true);
}

UnitConversionServiceFactory::UnitConversionServiceFactory()
    : ProfileKeyedServiceFactoryIOS("UnitConversionService",
                                    ProfileSelection::kOwnInstanceInIncognito) {
}

UnitConversionServiceFactory::~UnitConversionServiceFactory() {}

std::unique_ptr<KeyedService>
UnitConversionServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<UnitConversionService>();
}
