// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/saved_tab_groups/model/tab_group_service_factory.h"

#import "ios/chrome/browser/collaboration/model/features.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_service.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace {

// Creates the TabGroupService from `context`.
std::unique_ptr<KeyedService> CreateService(web::BrowserState* context) {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);

  if (!IsSharedTabGroupsJoinEnabled(profile) &&
      !IsSharedTabGroupsCreateEnabled(profile)) {
    return nullptr;
  }

  tab_groups::TabGroupSyncService* tab_group_sync_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile);
  return std::make_unique<TabGroupService>(profile, tab_group_sync_service);
}

}  // namespace

// static
TabGroupServiceFactory* TabGroupServiceFactory::GetInstance() {
  static base::NoDestructor<TabGroupServiceFactory> instance;
  return instance.get();
}

// static
TabGroupService* TabGroupServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<TabGroupService>(
      profile, /*create=*/true);
}

// static
BrowserStateKeyedServiceFactory::TestingFactory
TabGroupServiceFactory::GetDefaultFactory() {
  return base::BindRepeating(&CreateService);
}

TabGroupServiceFactory::TabGroupServiceFactory()
    : ProfileKeyedServiceFactoryIOS("TabGroupService",
                                    ServiceCreation::kCreateLazily,
                                    TestingCreation::kNoServiceForTests) {
  DependsOn(tab_groups::TabGroupSyncServiceFactory::GetInstance());
}

TabGroupServiceFactory::~TabGroupServiceFactory() = default;

std::unique_ptr<KeyedService> TabGroupServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return CreateService(context);
}
