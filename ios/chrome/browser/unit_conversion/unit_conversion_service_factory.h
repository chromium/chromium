// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UNIT_CONVERSION_UNIT_CONVERSION_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_UNIT_CONVERSION_UNIT_CONVERSION_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class UnitConversionService;

// Singleton that owns all UnitConversionServices and associates them with
// ProfileIOS.
class UnitConversionServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static UnitConversionService* GetForProfile(ProfileIOS* profile);
  static UnitConversionServiceFactory* GetInstance();

  UnitConversionServiceFactory(const UnitConversionServiceFactory&) = delete;
  UnitConversionServiceFactory& operator=(const UnitConversionServiceFactory&) =
      delete;

 private:
  friend class base::NoDestructor<UnitConversionServiceFactory>;
  UnitConversionServiceFactory();
  ~UnitConversionServiceFactory() override;
  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* state) const override;
};

#endif  // IOS_CHROME_BROWSER_UNIT_CONVERSION_UNIT_CONVERSION_SERVICE_FACTORY_H_
