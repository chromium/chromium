// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/plus_addresses/model/plus_address_setting_service_factory.h"

#import <memory>

#import "base/no_destructor.h"
#import "components/plus_addresses/settings/plus_address_setting_service_impl.h"
#import "components/plus_addresses/settings/plus_address_setting_sync_bridge.h"
#import "components/sync/model/data_type_store_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/sync/model/data_type_store_service_factory.h"

// static
PlusAddressSettingServiceFactory*
PlusAddressSettingServiceFactory::GetInstance() {
  static base::NoDestructor<PlusAddressSettingServiceFactory> instance;
  return instance.get();
}

// static
plus_addresses::PlusAddressSettingService*
PlusAddressSettingServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<plus_addresses::PlusAddressSettingService>(
          profile, true);
}

PlusAddressSettingServiceFactory::PlusAddressSettingServiceFactory()
    : ProfileKeyedServiceFactoryIOS("PlusAddressSettingServiceImpl",
                                    ProfileSelection::kRedirectedInIncognito) {
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
PlusAddressSettingServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  return std::make_unique<plus_addresses::PlusAddressSettingServiceImpl>(
      plus_addresses::PlusAddressSettingSyncBridge::CreateBridge(
          DataTypeStoreServiceFactory::GetForProfile(profile)
              ->GetStoreFactory()));
}
