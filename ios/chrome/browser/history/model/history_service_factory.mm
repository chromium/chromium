// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/history/model/history_service_factory.h"

#import <utility>

#import "components/history/core/browser/history_database_params.h"
#import "components/history/core/browser/history_service.h"
#import "components/history/core/browser/visit_delegate.h"
#import "components/history/core/common/pref_names.h"
#import "components/history/ios/browser/history_database_helper.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/prefs/pref_service.h"
#import "components/sync_device_info/device_info_sync_service.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/history/model/history_client_impl.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/sync/model/device_info_sync_service_factory.h"
#import "ios/chrome/common/channel_info.h"

namespace ios {

namespace {

// The maximum number of New Tab Page displays to show with synced segments
// data.
constexpr int kMaxSyncedNewTabPageDisplays = 5;

std::unique_ptr<KeyedService> BuildHistoryService(ProfileIOS* profile) {
  syncer::DeviceInfoSyncService* device_info_sync_service =
      DeviceInfoSyncServiceFactory::GetForProfile(profile);
  syncer::DeviceInfoTracker* device_info_tracker = nullptr;
  syncer::LocalDeviceInfoProvider* local_device_info_provider = nullptr;
  if (device_info_sync_service) {
    device_info_tracker = device_info_sync_service->GetDeviceInfoTracker();
    local_device_info_provider =
        device_info_sync_service->GetLocalDeviceInfoProvider();
  }

  auto history_service = std::make_unique<history::HistoryService>(
      std::make_unique<HistoryClientImpl>(
          BookmarkModelFactory::GetForProfile(profile)),
      /*visit_delegate=*/nullptr, device_info_tracker,
      local_device_info_provider);
  if (!history_service->Init(history::HistoryDatabaseParamsForPath(
          profile->GetStatePath(), GetChannel()))) {
    return nullptr;
  }

  if (device_info_sync_service) {
    PrefService* pref_service = profile->GetPrefs();

    const int display_count =
        pref_service->GetInteger(prefs::kIosSyncSegmentsNewTabPageDisplayCount);

    history_service->SetCanAddForeignVisitsToSegmentsOnBackend(
        display_count < kMaxSyncedNewTabPageDisplays);
  }

  return history_service;
}

}  // namespace

// static
history::HistoryService* HistoryServiceFactory::GetForProfile(
    ProfileIOS* profile,
    ServiceAccessType access_type) {
  // If saving history is disabled, only allow explicit access.
  if (access_type != ServiceAccessType::EXPLICIT_ACCESS &&
      profile->GetPrefs()->GetBoolean(prefs::kSavingBrowserHistoryDisabled)) {
    return nullptr;
  }

  return GetInstance()->GetServiceForProfileAs<history::HistoryService>(
      profile, /*create=*/true);
}

// static
HistoryServiceFactory* HistoryServiceFactory::GetInstance() {
  static base::NoDestructor<HistoryServiceFactory> instance;
  return instance.get();
}

// static
HistoryServiceFactory::TestingFactory
HistoryServiceFactory::GetDefaultFactory() {
  return base::BindOnce(&BuildHistoryService);
}

HistoryServiceFactory::HistoryServiceFactory()
    : ProfileKeyedServiceFactoryIOS("HistoryService",
                                    ProfileSelection::kRedirectedInIncognito,
                                    TestingCreation::kNoServiceForTests) {
  DependsOn(BookmarkModelFactory::GetInstance());
  DependsOn(DeviceInfoSyncServiceFactory::GetInstance());
}

HistoryServiceFactory::~HistoryServiceFactory() = default;

std::unique_ptr<KeyedService> HistoryServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return BuildHistoryService(profile);
}

}  // namespace ios
