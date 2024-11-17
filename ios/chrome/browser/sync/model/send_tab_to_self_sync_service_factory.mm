// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sync/model/send_tab_to_self_sync_service_factory.h"

#import "base/no_destructor.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#import "components/sync_device_info/device_info_sync_service.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/sync/model/data_type_store_service_factory.h"
#import "ios/chrome/browser/sync/model/device_info_sync_service_factory.h"
#import "ios/chrome/common/channel_info.h"

using send_tab_to_self::SendTabToSelfSyncService;

std::unique_ptr<KeyedService> BuildSendTabToSelfService(
    web::BrowserState* context) {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);

  syncer::OnceDataTypeStoreFactory store_factory =
      DataTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory();

  history::HistoryService* history_service =
      ios::HistoryServiceFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS);

  syncer::DeviceInfoTracker* device_info_tracker =
      DeviceInfoSyncServiceFactory::GetForProfile(profile)
          ->GetDeviceInfoTracker();

  return std::make_unique<SendTabToSelfSyncService>(
      GetChannel(), std::move(store_factory), history_service,
      profile->GetPrefs(), device_info_tracker);
}

// static
SendTabToSelfSyncServiceFactory*
SendTabToSelfSyncServiceFactory::GetInstance() {
  static base::NoDestructor<SendTabToSelfSyncServiceFactory> instance;
  return instance.get();
}

// static
SendTabToSelfSyncService* SendTabToSelfSyncServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return static_cast<SendTabToSelfSyncService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
BrowserStateKeyedServiceFactory::TestingFactory
SendTabToSelfSyncServiceFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildSendTabToSelfService);
}

SendTabToSelfSyncServiceFactory::SendTabToSelfSyncServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "SendTabToSelfSyncService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
  DependsOn(DeviceInfoSyncServiceFactory::GetInstance());
  DependsOn(ios::HistoryServiceFactory::GetInstance());
}

SendTabToSelfSyncServiceFactory::~SendTabToSelfSyncServiceFactory() {}

std::unique_ptr<KeyedService>
SendTabToSelfSyncServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return BuildSendTabToSelfService(context);
}
