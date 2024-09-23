// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/unit_conversion/unit_conversion_service_factory.h"

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/unit_conversion/unit_conversion_service.h"

// static
UnitConversionServiceFactory* UnitConversionServiceFactory::GetInstance() {
  static base::NoDestructor<UnitConversionServiceFactory> instance;
  return instance.get();
}

// static
UnitConversionService* UnitConversionServiceFactory::GetForProfile(
    ProfileIOS* const state) {
  return static_cast<UnitConversionService*>(
      GetInstance()->GetServiceForBrowserState(state, true));
}

UnitConversionServiceFactory::UnitConversionServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "UnitConversionService",
          BrowserStateDependencyManager::GetInstance()) {}

UnitConversionServiceFactory::~UnitConversionServiceFactory() {}

std::unique_ptr<KeyedService>
UnitConversionServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* const state) const {
  return std::make_unique<UnitConversionService>();
}

web::BrowserState* UnitConversionServiceFactory::GetBrowserStateToUse(
    web::BrowserState* const state) const {
  // The incognito has distinct instance of `UnitConversionService`.
  return GetBrowserStateOwnInstanceInIncognito(state);
}
