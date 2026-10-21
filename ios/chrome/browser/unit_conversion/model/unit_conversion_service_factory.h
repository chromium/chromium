// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UNIT_CONVERSION_MODEL_UNIT_CONVERSION_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_UNIT_CONVERSION_MODEL_UNIT_CONVERSION_SERVICE_FACTORY_H_

#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"
#import "ios/chrome/browser/unit_conversion/model/unit_conversion_service.h"

// Singleton that owns all UnitConversionServices and associates them with
// ProfileIOS.
class UnitConversionServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<UnitConversionServiceFactory,
                                                UnitConversionService> {
 public:
  explicit UnitConversionServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_UNIT_CONVERSION_MODEL_UNIT_CONVERSION_SERVICE_FACTORY_H_
