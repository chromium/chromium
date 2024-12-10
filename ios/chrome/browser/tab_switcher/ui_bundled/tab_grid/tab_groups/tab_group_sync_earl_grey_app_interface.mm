// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_group_sync_earl_grey_app_interface.h"

#import "base/strings/string_number_conversions.h"
#import "components/saved_tab_groups/test_support/fake_tab_group_sync_service.h"
#import "components/sync/service/sync_service.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/test/app/sync_test_util.h"

namespace {

// Returns the first regular (= non-incognito) profile from the loaded browser
// states.
ProfileIOS* GetRegularProfile() {
  for (ProfileIOS* profile :
       GetApplicationContext()->GetProfileManager()->GetLoadedProfiles()) {
    if (!profile->IsOffTheRecord()) {
      return profile;
    }
  }
  return nullptr;
}

// Returns the tab group sync service from the first regular profile.
tab_groups::TabGroupSyncService* GetFakeTabGroupSyncService() {
  CHECK(IsTabGroupSyncEnabled());
  return tab_groups::TabGroupSyncServiceFactory::GetForProfile(
      GetRegularProfile());
}

// Returns the sync service from the first regular profile.
syncer::SyncService* GetSyncService() {
  return SyncServiceFactory::GetForProfile(GetRegularProfile());
}

// Creates a saved tab belonging `saved_tab_group_id` group.
tab_groups::SavedTabGroupTab CreateTab(const base::Uuid& saved_tab_group_id) {
  tab_groups::SavedTabGroupTab saved_tab(GURL("https://google.com"), u"Google",
                                         saved_tab_group_id,
                                         /*position=*/0);
  return saved_tab;
}

// Creates a saved tab group with a given `title`, `saved_tabs`, `group_id` and
// the orange group color.
tab_groups::SavedTabGroup CreateGroup(
    std::u16string title,
    const std::vector<tab_groups::SavedTabGroupTab>& saved_tabs,
    const base::Uuid& group_id) {
  tab_groups::SavedTabGroup saved_group(
      title, tab_groups::TabGroupColorId::kOrange, saved_tabs,
      /*position=*/std::nullopt, group_id);
  return saved_group;
}

}  // namespace

@implementation TabGroupSyncEarlGreyAppInterface

+ (void)prepareFakeSavedTabGroups:(NSInteger)numberOfGroups {
  CHECK(IsTabGroupSyncEnabled());
  for (NSInteger i = 0; i < numberOfGroups; i++) {
    base::Uuid group_id = base::Uuid::GenerateRandomV4();
    std::vector<tab_groups::SavedTabGroupTab> tabs;
    tab_groups::SavedTabGroupTab tab = CreateTab(group_id);
    tabs.push_back(tab);
    chrome_test_util::AddTabToFakeServer(tab);
    chrome_test_util::AddGroupToFakeServer(CreateGroup(
        base::NumberToString16(i) + u"RemoteGroup", tabs, group_id));
  }

  GetSyncService()->TriggerRefresh({syncer::SAVED_TAB_GROUP});
}

+ (void)removeAtIndex:(unsigned int)index {
  CHECK(IsTabGroupSyncEnabled());
  std::vector<tab_groups::SavedTabGroup> groups =
      GetFakeTabGroupSyncService()->GetAllGroups();
  tab_groups::SavedTabGroup groupToRemove = groups[index];
  chrome_test_util::DeleteTabOrGroupFromFakeServer(groupToRemove.saved_guid());

  GetSyncService()->TriggerRefresh({syncer::SAVED_TAB_GROUP});
}

+ (void)cleanup {
  CHECK(IsTabGroupSyncEnabled());

  std::vector<tab_groups::SavedTabGroup> groups =
      GetFakeTabGroupSyncService()->GetAllGroups();
  for (const tab_groups::SavedTabGroup& group : groups) {
    chrome_test_util::DeleteTabOrGroupFromFakeServer(group.saved_guid());
  }

  GetSyncService()->TriggerRefresh({syncer::SAVED_TAB_GROUP});
}

+ (int)countOfSavedTabGroups {
  CHECK(IsTabGroupSyncEnabled());
  tab_groups::TabGroupSyncService* tabGroupSyncService =
      GetFakeTabGroupSyncService();
  return tabGroupSyncService->GetAllGroups().size();
}

@end
