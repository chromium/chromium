// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sync/model/send_tab_to_self_sync_service_factory.h"

#import <utility>

#import "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#import "components/sync_device_info/device_info_sync_service.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/sync/model/data_type_store_service_factory.h"
#import "ios/chrome/browser/sync/model/device_info_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/session_sync_service_factory.h"
#import "ios/chrome/common/channel_info.h"

using send_tab_to_self::SendTabToSelfSyncService;

namespace {

std::unique_ptr<KeyedService> BuildSendTabToSelfService(ProfileIOS* profile) {
  syncer::OnceDataTypeStoreFactory store_factory =
      DataTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory();

  history::HistoryService* history_service =
      ios::HistoryServiceFactory::GetForProfile(profile);

  syncer::DeviceInfoTracker* device_info_tracker =
      DeviceInfoSyncServiceFactory::GetForProfile(profile)
          ->GetDeviceInfoTracker();

  sync_sessions::SessionSyncService* session_sync_service =
      SessionSyncServiceFactory::GetForProfile(profile);

  return std::make_unique<SendTabToSelfSyncService>(
      GetChannel(), std::move(store_factory), history_service,
      profile->GetPrefs(), device_info_tracker, session_sync_service);
}

}  // anonymous namespace

// static
SendTabToSelfSyncServiceFactory::TestingFactory
SendTabToSelfSyncServiceFactory::GetDefaultFactory() {
  return base::BindOnce(&BuildSendTabToSelfService);
}

SendTabToSelfSyncServiceFactory::SendTabToSelfSyncServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(std::move(key),
                                         "SendTabToSelfSyncService") {
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
  DependsOn(DeviceInfoSyncServiceFactory::GetInstance());
  DependsOn(ios::HistoryServiceFactory::GetInstance());
  DependsOn(SessionSyncServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
SendTabToSelfSyncServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return BuildSendTabToSelfService(profile);
}
