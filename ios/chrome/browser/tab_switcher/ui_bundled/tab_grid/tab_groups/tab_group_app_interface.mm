// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_group_app_interface.h"

#import "base/strings/string_number_conversions.h"
#import "components/data_sharing/public/data_sharing_service.h"
#import "components/data_sharing/public/group_data.h"
#import "components/data_sharing/test_support/mock_preview_server_proxy.h"
#import "components/saved_tab_groups/test_support/fake_tab_group_sync_service.h"
#import "components/sync/service/sync_service.h"
#import "ios/chrome/browser/collaboration/model/features.h"
#import "ios/chrome/browser/data_sharing/model/data_sharing_service_factory.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/share_kit/model/share_kit_service_factory.h"
#import "ios/chrome/browser/share_kit/model/test_share_kit_service.h"
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
tab_groups::TabGroupSyncService* GetTabGroupSyncService() {
  CHECK(IsTabGroupSyncEnabled());
  return tab_groups::TabGroupSyncServiceFactory::GetForProfile(
      GetRegularProfile());
}

// Returns the data sharing service from the first regular profile.
data_sharing::DataSharingService* GetDataSharingService() {
  ProfileIOS* profile = GetRegularProfile();
  CHECK(IsSharedTabGroupsJoinEnabled(profile));
  return data_sharing::DataSharingServiceFactory::GetForProfile(profile);
}

// Returns the sync service from the first regular profile.
syncer::SyncService* GetSyncService() {
  return SyncServiceFactory::GetForProfile(GetRegularProfile());
}

// Returns the share kit service from the first regular profile.
TestShareKitService* GetShareKitService() {
  ProfileIOS* profile = GetRegularProfile();
  CHECK(IsSharedTabGroupsJoinEnabled(profile));
  CHECK(IsSharedTabGroupsCreateEnabled(profile));
  return static_cast<TestShareKitService*>(
      ShareKitServiceFactory::GetForProfile(profile));
}

// Returns a new SharedTabGroupPreview.
data_sharing::SharedTabGroupPreview CreateTabGroupPreview() {
  data_sharing::SharedTabGroupPreview preview;
  preview.title = "my group title";
  for (int i = 0; i < 3; i++) {
    preview.tabs.push_back(data_sharing::TabPreview(
        GURL("https://google.com/" + base::NumberToString(i))));
  }
  return preview;
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

// testing::InvokeArgument<N> does not work with base::OnceCallback. Use this
// gmock action template to invoke base::OnceCallback. `k` is the k-th argument
// and `T` is the callback's type.
ACTION_TEMPLATE(InvokeCallbackArgument,
                HAS_2_TEMPLATE_PARAMS(int, k, typename, T),
                AND_1_VALUE_PARAMS(p0)) {
  std::move(const_cast<T&>(std::get<k>(args))).Run(p0);
}

}  // namespace

@implementation TabGroupAppInterface

+ (void)prepareFakeSyncedTabGroups:(NSInteger)numberOfGroups {
  CHECK(IsTabGroupSyncEnabled());
  for (NSInteger i = 0; i < numberOfGroups; i++) {
    base::Uuid groupID = base::Uuid::GenerateRandomV4();
    std::vector<tab_groups::SavedTabGroupTab> tabs;
    tab_groups::SavedTabGroupTab tab = CreateTab(groupID);
    tabs.push_back(tab);
    chrome_test_util::AddTabToFakeServer(tab);
    chrome_test_util::AddGroupToFakeServer(
        CreateGroup(base::NumberToString16(i) + u"RemoteGroup", tabs, groupID));
  }

  GetSyncService()->TriggerRefresh({syncer::SAVED_TAB_GROUP});
}

+ (void)prepareFakeSharedTabGroups:(NSInteger)numberOfGroups {
  CHECK(IsTabGroupSyncEnabled());
  for (NSInteger i = 0; i < numberOfGroups; i++) {
    NSString* collaborationID =
        [NSString stringWithFormat:@"CollaborationIDd%ld", i];

    // Create a shared tab group in the fake server. The user (`fakeIdentity1`)
    // will join the group as a member.
    GetShareKitService()->CreateSharedTabGroupInFakeServer(collaborationID);
  }

  GetSyncService()->TriggerRefresh({syncer::COLLABORATION_GROUP});
}

+ (void)removeAtIndex:(unsigned int)index {
  CHECK(IsTabGroupSyncEnabled());
  std::vector<tab_groups::SavedTabGroup> groups =
      GetTabGroupSyncService()->GetAllGroups();
  tab_groups::SavedTabGroup groupToRemove = groups[index];
  chrome_test_util::DeleteTabOrGroupFromFakeServer(groupToRemove.saved_guid());

  GetSyncService()->TriggerRefresh({syncer::SAVED_TAB_GROUP});
}

+ (void)cleanup {
  CHECK(IsTabGroupSyncEnabled());

  std::vector<tab_groups::SavedTabGroup> groups =
      GetTabGroupSyncService()->GetAllGroups();
  for (const tab_groups::SavedTabGroup& group : groups) {
    chrome_test_util::DeleteTabOrGroupFromFakeServer(group.saved_guid());
  }

  GetSyncService()->TriggerRefresh({syncer::SAVED_TAB_GROUP});
}

+ (int)countOfSavedTabGroups {
  CHECK(IsTabGroupSyncEnabled());
  tab_groups::TabGroupSyncService* tabGroupSyncService =
      GetTabGroupSyncService();
  return tabGroupSyncService->GetAllGroups().size();
}

+ (void)mockSharedEntitiesPreview {
  data_sharing::DataSharingService* dataSharingService =
      GetDataSharingService();
  data_sharing::MockPreviewServerProxy* mockPreviewProxy =
      static_cast<data_sharing::MockPreviewServerProxy*>(
          dataSharingService->GetPreviewServerProxyForTesting());

  data_sharing::SharedDataPreview sharedDataPreview;
  sharedDataPreview.shared_tab_group_preview = CreateTabGroupPreview();
  data_sharing::DataSharingService::SharedDataPreviewOrFailureOutcome outcome =
      sharedDataPreview;
  ON_CALL(*mockPreviewProxy,
          GetSharedDataPreview(testing::_, testing::_, testing::_))
      .WillByDefault(
          InvokeCallbackArgument<
              2,
              base::OnceCallback<void(const data_sharing::DataSharingService::
                                          SharedDataPreviewOrFailureOutcome&)>>(
              outcome));
}

@end
