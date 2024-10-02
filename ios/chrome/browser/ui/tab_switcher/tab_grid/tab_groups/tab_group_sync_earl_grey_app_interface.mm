// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_group_sync_earl_grey_app_interface.h"

#import "components/saved_tab_groups/test_support/fake_tab_group_sync_service.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"

namespace {

// Returns the first regular (= non-incognito) browser from the loaded browser
// states.
ProfileIOS* GetRegularBrowser() {
  for (ProfileIOS* profile :
       GetApplicationContext()->GetProfileManager()->GetLoadedProfiles()) {
    if (!profile->IsOffTheRecord()) {
      return profile;
    }
  }
  return nullptr;
}

// Returns the fake sync service from the first regular browser.
tab_groups::FakeTabGroupSyncService* GetFakeTabGroupSyncService() {
  CHECK(IsTabGroupSyncEnabled());
  return static_cast<tab_groups::FakeTabGroupSyncService*>(
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(
          GetRegularBrowser()));
}

}  // namespace

@implementation TabGroupSyncEarlGreyAppInterface

+ (void)prepareFakeSavedTabGroups {
  CHECK(IsTabGroupSyncEnabled());
  tab_groups::FakeTabGroupSyncService* tabGroupSyncService =
      GetFakeTabGroupSyncService();
  tabGroupSyncService->PrepareFakeSavedTabGroups();
}

+ (void)removeAtIndex:(unsigned int)index {
  CHECK(IsTabGroupSyncEnabled());
  tab_groups::FakeTabGroupSyncService* tabGroupSyncService =
      GetFakeTabGroupSyncService();
  tabGroupSyncService->RemoveGroupAtIndex(index);
}

+ (void)cleanup {
  CHECK(IsTabGroupSyncEnabled());
  tab_groups::FakeTabGroupSyncService* tabGroupSyncService =
      GetFakeTabGroupSyncService();
  tabGroupSyncService->ClearGroups();
}

+ (int)countOfSavedTabGroups {
  CHECK(IsTabGroupSyncEnabled());
  tab_groups::FakeTabGroupSyncService* tabGroupSyncService =
      GetFakeTabGroupSyncService();
  return tabGroupSyncService->GetAllGroups().size();
}

@end
