// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UNIT_CONVERSION_MODEL_UNIT_CONVERSION_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_UNIT_CONVERSION_MODEL_UNIT_CONVERSION_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;
class UnitConversionService;

// Singleton that owns all UnitConversionServices and associates them with
// ProfileIOS.
class UnitConversionServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static UnitConversionService* GetForProfile(ProfileIOS* profile);
  static UnitConversionServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<UnitConversionServiceFactory>;

  UnitConversionServiceFactory();
  ~UnitConversionServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_UNIT_CONVERSION_MODEL_UNIT_CONVERSION_SERVICE_FACTORY_H_
