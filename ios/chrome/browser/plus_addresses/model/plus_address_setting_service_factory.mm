// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/plus_addresses/model/plus_address_setting_service_factory.h"

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/plus_addresses/settings/plus_address_setting_service.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"

// static
PlusAddressSettingServiceFactory*
PlusAddressSettingServiceFactory::GetInstance() {
  static base::NoDestructor<PlusAddressSettingServiceFactory> instance;
  return instance.get();
}

// static
plus_addresses::PlusAddressSettingService*
PlusAddressSettingServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<plus_addresses::PlusAddressSettingService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

PlusAddressSettingServiceFactory::PlusAddressSettingServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "PlusAddressSettingService",
          BrowserStateDependencyManager::GetInstance()) {}

std::unique_ptr<KeyedService>
PlusAddressSettingServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return std::make_unique<plus_addresses::PlusAddressSettingService>();
}

web::BrowserState* PlusAddressSettingServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}
