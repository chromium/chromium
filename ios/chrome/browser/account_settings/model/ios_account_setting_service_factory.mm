// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/account_settings/model/ios_account_setting_service_factory.h"

#import "base/functional/callback_helpers.h"
#import "components/account_settings/account_setting_service_impl.h"
#import "components/account_settings/account_setting_sync_bridge.h"
#import "components/sync/base/features.h"
#import "components/sync/model/client_tag_based_data_type_processor.h"
#import "components/sync/model/data_type_store_service.h"
#import "ios/chrome/browser/sync/model/data_type_store_service_factory.h"

// static
IOSAccountSettingServiceFactory*
IOSAccountSettingServiceFactory::GetInstance() {
  static base::NoDestructor<IOSAccountSettingServiceFactory> instance;
  return instance.get();
}

// static
account_settings::AccountSettingService*
IOSAccountSettingServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<account_settings::AccountSettingService>(
          profile, /*create=*/true);
}

IOSAccountSettingServiceFactory::IOSAccountSettingServiceFactory()
    : ProfileKeyedServiceFactoryIOS("AccountSettingService",
                                    ProfileSelection::kOwnInstanceInIncognito) {
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
}

IOSAccountSettingServiceFactory::~IOSAccountSettingServiceFactory() = default;

std::unique_ptr<KeyedService>
IOSAccountSettingServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  if (!base::FeatureList::IsEnabled(syncer::kSyncAccountSettings)) {
    return nullptr;
  }

  return std::make_unique<account_settings::AccountSettingServiceImpl>(
      std::make_unique<account_settings::AccountSettingSyncBridge>(
          std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
              syncer::ACCOUNT_SETTING,
              /*dump_stack=*/base::DoNothing()),
          DataTypeStoreServiceFactory::GetForProfile(profile)
              ->GetStoreFactory()));
}
