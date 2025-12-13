// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_group_app_interface.h"

#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/collaboration/public/pref_names.h"
#import "components/data_sharing/public/data_sharing_service.h"
#import "components/data_sharing/public/features.h"
#import "components/data_sharing/public/group_data.h"
#import "components/data_sharing/test_support/mock_preview_server_proxy.h"
#import "components/policy/core/common/policy_map.h"
#import "components/policy/policy_constants.h"
#import "components/saved_tab_groups/public/saved_tab_group.h"
#import "components/saved_tab_groups/public/saved_tab_group_tab.h"
#import "components/saved_tab_groups/test_support/fake_tab_group_sync_service.h"
#import "ios/chrome/browser/collaboration/model/collaboration_service_factory.h"
#import "ios/chrome/browser/collaboration/model/features.h"
#import "ios/chrome/browser/data_sharing/model/data_sharing_service_factory.h"
#import "ios/chrome/browser/policy/model/test_platform_policy_provider.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/share_kit/model/share_kit_service_factory.h"
#import "ios/chrome/browser/share_kit/model/test_share_kit_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/sync_test_util.h"

using collaboration::prefs::SharedTabGroupsManagedAccountSetting;

namespace {

// Returns the tab group sync service from the first regular profile.
tab_groups::TabGroupSyncService* GetTabGroupSyncService() {
  ProfileIOS* profile = chrome_test_util::GetOriginalProfile();
  return tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile);
}

// Returns the data sharing service from the first regular profile.
data_sharing::DataSharingService* GetDataSharingService() {
  ProfileIOS* profile = chrome_test_util::GetOriginalProfile();
  collaboration::CollaborationService* collaboration_service =
      collaboration::CollaborationServiceFactory::GetForProfile(profile);
  CHECK(IsSharedTabGroupsJoinEnabled(collaboration_service));
  return data_sharing::DataSharingServiceFactory::GetForProfile(profile);
}

// Returns the share kit service from the first regular profile.
TestShareKitService* GetShareKitService() {
  ProfileIOS* profile = chrome_test_util::GetOriginalProfile();
  collaboration::CollaborationService* collaboration_service =
      collaboration::CollaborationServiceFactory::GetForProfile(profile);
  CHECK(IsSharedTabGroupsJoinEnabled(collaboration_service));
  CHECK(IsSharedTabGroupsCreateEnabled(collaboration_service));
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
  for (NSInteger i = 0; i < numberOfGroups; i++) {
    base::Uuid groupID = base::Uuid::GenerateRandomV4();
    std::vector<tab_groups::SavedTabGroupTab> tabs;
    tab_groups::SavedTabGroupTab tab = CreateTab(groupID);
    tabs.push_back(tab);
    chrome_test_util::AddTabToFakeServer(tab);
    chrome_test_util::AddGroupToFakeServer(
        CreateGroup(base::NumberToString16(i) + u"RemoteGroup", tabs, groupID));
  }

  chrome_test_util::TriggerSyncCycle(syncer::SAVED_TAB_GROUP);
}

+ (void)prepareFakeSharedTabGroups:(NSInteger)numberOfGroups
                           asOwner:(BOOL)owner
                               url:(NSString*)url {
  GURL gurl(base::SysNSStringToUTF8(url));
  for (NSInteger i = 0; i < numberOfGroups; i++) {
    NSString* collaborationID =
        [NSString stringWithFormat:@"CollaborationID%ld", i];

    // Create a shared tab group in the fake server.
    GetShareKitService()->CreateSharedTabGroupInFakeServer(
        owner, collaborationID, gurl);
  }

  chrome_test_util::TriggerSyncCycle(syncer::COLLABORATION_GROUP);
}

+ (void)removeAtIndex:(unsigned int)index {
  std::vector<tab_groups::SavedTabGroup> groups =
      GetTabGroupSyncService()->GetAllGroups();
  tab_groups::SavedTabGroup groupToRemove = groups[index];

  chrome_test_util::DeleteTabOrGroupFromFakeServer(groupToRemove.saved_guid());
  chrome_test_util::TriggerSyncCycle(syncer::SAVED_TAB_GROUP);

  // When a group is shared, the fake server stores the data of the group as
  // syncer::SAVED_TAB_GROUP and syncer::SHARED_TAB_GROUP_DATA. Remove the both
  // data from the fake server.
  if (groupToRemove.is_shared_tab_group()) {
    chrome_test_util::DeleteSharedGroupFromFakeServer(
        groupToRemove.saved_guid());
    chrome_test_util::TriggerSyncCycle(syncer::SHARED_TAB_GROUP_DATA);
  }
}

+ (void)cleanup {
  std::vector<tab_groups::SavedTabGroup> groups =
      GetTabGroupSyncService()->GetAllGroups();
  for (unsigned int i = 0; i < groups.size(); i++) {
    [self removeAtIndex:i];
  }

  chrome_test_util::TriggerSyncCycle(syncer::SAVED_TAB_GROUP);
}

+ (int)countOfSavedTabGroups {
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

+ (void)addSharedTabToGroupAtIndex:(unsigned int)index {
  std::vector<tab_groups::SavedTabGroup> groups =
      GetTabGroupSyncService()->GetAllGroups();
  tab_groups::SavedTabGroup group = groups[index];
  CHECK(group.collaboration_id().has_value());

  tab_groups::SavedTabGroupTab tab(GURL("https://example.com"), u"Example",
                                   group.saved_guid(), 1);
  chrome_test_util::AddSharedTabToFakeServer(
      tab, group.collaboration_id().value());
  chrome_test_util::TriggerSyncCycle(syncer::SHARED_TAB_GROUP_DATA);
}

+ (NSString*)activityLogsURL {
  return base::SysUTF8ToNSString(
      data_sharing::features::kActivityLogsURL.Get());
}

+ (void)setSharedTabGroupsManagedAccountPolicyEnabled:
    (BOOL)managedAccountPolicyEnabled {
  policy::PolicyMap values;
  SharedTabGroupsManagedAccountSetting setting =
      managedAccountPolicyEnabled
          ? SharedTabGroupsManagedAccountSetting::kEnabled
          : SharedTabGroupsManagedAccountSetting::kDisabled;
  values.Set(policy::key::kTabGroupSharingSettings,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD,
             base::Value(static_cast<int>(setting)),
             /*external_data_fetcher=*/nullptr);
  GetTestPlatformPolicyProvider()->UpdateChromePolicy(values);
}

+ (BOOL)isAllowedToJoinTabGroups {
  ProfileIOS* profile = chrome_test_util::GetOriginalProfile();
  collaboration::CollaborationService* collaboration_service =
      collaboration::CollaborationServiceFactory::GetForProfile(profile);
  return IsSharedTabGroupsJoinEnabled(collaboration_service);
}

+ (BOOL)isAllowedToShareTabGroups {
  ProfileIOS* profile = chrome_test_util::GetOriginalProfile();
  collaboration::CollaborationService* collaboration_service =
      collaboration::CollaborationServiceFactory::GetForProfile(profile);
  return IsSharedTabGroupsCreateEnabled(collaboration_service);
}

@end
