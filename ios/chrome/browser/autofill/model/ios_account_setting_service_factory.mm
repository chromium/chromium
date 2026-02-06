// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/ios_account_setting_service_factory.h"

#import "base/functional/callback_helpers.h"
#import "components/autofill/core/browser/webdata/account_settings/account_setting_service.h"
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
autofill::AccountSettingService* IOSAccountSettingServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<autofill::AccountSettingService>(
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

  return std::make_unique<autofill::AccountSettingService>(
      std::make_unique<autofill::AccountSettingSyncBridge>(
          std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
              syncer::ACCOUNT_SETTING,
              /*dump_stack=*/base::DoNothing()),
          DataTypeStoreServiceFactory::GetForProfile(profile)
              ->GetStoreFactory()));
}
