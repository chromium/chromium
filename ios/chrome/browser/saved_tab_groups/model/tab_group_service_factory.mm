// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/saved_tab_groups/model/tab_group_service_factory.h"

#import "base/check.h"
#import "ios/chrome/browser/collaboration/model/collaboration_service_factory.h"
#import "ios/chrome/browser/collaboration/model/features.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_service.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace {

// Creates the TabGroupService from `context`.
std::unique_ptr<KeyedService> CreateService(ProfileIOS* profile) {
  CHECK(!profile->IsOffTheRecord());

  collaboration::CollaborationService* collaboration_service =
      collaboration::CollaborationServiceFactory::GetForProfile(profile);
  if (!IsSharedTabGroupsJoinEnabled(collaboration_service) &&
      !IsSharedTabGroupsCreateEnabled(collaboration_service)) {
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

TabGroupServiceFactory::TabGroupServiceFactory()
    : ProfileKeyedServiceFactoryIOS("TabGroupService") {
  DependsOn(collaboration::CollaborationServiceFactory::GetInstance());
  DependsOn(tab_groups::TabGroupSyncServiceFactory::GetInstance());
}

TabGroupServiceFactory::~TabGroupServiceFactory() = default;

std::unique_ptr<KeyedService> TabGroupServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return CreateService(profile);
}
