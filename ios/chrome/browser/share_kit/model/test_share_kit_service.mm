// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/share_kit/model/test_share_kit_service.h"

#import "base/functional/callback_helpers.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/single_thread_task_runner.h"
#import "components/data_sharing/public/data_sharing_service.h"
#import "components/saved_tab_groups/public/collaboration_finder.h"
#import "components/saved_tab_groups/public/saved_tab_group.h"
#import "components/saved_tab_groups/public/tab_group_sync_service.h"
#import "ios/chrome/browser/data_sharing/model/data_sharing_sdk_delegate_ios.h"
#import "ios/chrome/browser/data_sharing/model/data_sharing_ui_delegate_ios.h"
#import "ios/chrome/browser/share_kit/model/fake_share_kit_flow_view_controller.h"
#import "ios/chrome/browser/share_kit/model/share_kit_delete_configuration.h"
#import "ios/chrome/browser/share_kit/model/share_kit_join_configuration.h"
#import "ios/chrome/browser/share_kit/model/share_kit_leave_configuration.h"
#import "ios/chrome/browser/share_kit/model/share_kit_manage_configuration.h"
#import "ios/chrome/browser/share_kit/model/share_kit_read_configuration.h"
#import "ios/chrome/browser/share_kit/model/share_kit_share_group_configuration.h"
#import "ios/chrome/browser/share_kit/model/test_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/test/app/sync_test_util.h"

using data_sharing::GroupId;
using data_sharing_pb::GroupData;
using data_sharing_pb::GroupMember;
using data_sharing_pb::MemberRole;

namespace {

// Delay to observe when deleting a shared tab group from the server.
constexpr base::TimeDelta kDeleteGroupDelay = base::Seconds(0.5);

// Creates a saved tab belonging to `group_guid` group.
tab_groups::SavedTabGroupTab CreateTab(const base::Uuid& group_guid) {
  tab_groups::SavedTabGroupTab saved_tab(GURL("https://google.com"), u"Google",
                                         group_guid,
                                         /*position=*/0);
  return saved_tab;
}

// Creates a saved tab group with the given parameters.
tab_groups::SavedTabGroup CreateGroup(
    std::u16string title,
    const std::vector<tab_groups::SavedTabGroupTab>& saved_tabs,
    const base::Uuid& group_guid) {
  tab_groups::SavedTabGroup saved_group(
      title, tab_groups::TabGroupColorId::kOrange, saved_tabs,
      /*position=*/std::nullopt, /*saved_guid=*/group_guid);
  return saved_group;
}

// Creates a group member with the given `gaia_id` and `member_role`.
data_sharing_pb::GroupMember CreateGroupMember(NSString* gaia_id,
                                               MemberRole member_role) {
  GroupMember member;
  if (member_role == data_sharing_pb::MEMBER_ROLE_OWNER) {
    member.set_display_name("Owner");
    member.set_email("owner@mail.com");
  } else {
    member.set_display_name("Member");
    member.set_email("member@mail.com");
  }
  member.set_gaia_id(base::SysNSStringToUTF8(gaia_id));
  member.set_avatar_url("chrome://newtab");
  member.set_given_name("Given Name");
  member.set_role(member_role);
  return member;
}

// Creates group data for a group containing two users.
// The first user has the given `member_role`.
GroupData CreateGroupData(MemberRole member_role, NSString* collaboration_id) {
  data_sharing_pb::GroupData group_data;
  group_data.set_group_id(base::SysNSStringToUTF8(collaboration_id));
  group_data.set_display_name("Display Name");
  *group_data.add_members() =
      CreateGroupMember([FakeSystemIdentity fakeIdentity1].gaiaID, member_role);
  MemberRole member_role2 = member_role == data_sharing_pb::MEMBER_ROLE_OWNER
                                ? data_sharing_pb::MEMBER_ROLE_MEMBER
                                : data_sharing_pb::MEMBER_ROLE_OWNER;
  *group_data.add_members() = CreateGroupMember(
      [FakeSystemIdentity fakeIdentity2].gaiaID, member_role2);
  group_data.set_access_token("fake_access_token");
  return group_data;
}

}  // namespace

TestShareKitService::TestShareKitService(
    data_sharing::DataSharingService* data_sharing_service,
    collaboration::CollaborationService* collaboration_service,
    tab_groups::TabGroupSyncService* tab_group_sync_service)
    : data_sharing_service_(data_sharing_service),
      tab_group_sync_service_(tab_group_sync_service) {
  if (data_sharing_service_) {
    std::unique_ptr<data_sharing::DataSharingUIDelegateIOS> ui_delegate =
        std::make_unique<data_sharing::DataSharingUIDelegateIOS>(
            this, collaboration_service);
    data_sharing_service_->SetUIDelegate(std::move(ui_delegate));

    std::unique_ptr<data_sharing::DataSharingSDKDelegateIOS> sdk_delegate =
        std::make_unique<data_sharing::DataSharingSDKDelegateIOS>(this);
    data_sharing_service_->SetSDKDelegate(std::move(sdk_delegate));
  }
}

TestShareKitService::~TestShareKitService() {}

bool TestShareKitService::IsSupported() const {
  return true;
}

void TestShareKitService::PrimaryAccountChanged() {
  // No-op for testing.
}

void TestShareKitService::CancelSession(NSString* session_id) {}

NSString* TestShareKitService::ShareTabGroup(
    ShareKitShareGroupConfiguration* config) {
  const TabGroup* tab_group = config.tabGroup;
  if (!tab_group) {
    return nil;
  }
  is_owner_ = true;
  tab_groups::LocalTabGroupID tab_group_id = tab_group->tab_group_id();

  FakeShareKitFlowViewController* viewController =
      [[FakeShareKitFlowViewController alloc]
          initWithType:FakeShareKitFlowType::kShare];
  viewController.completionBlock = config.completion;

  // Set the shared group completion block.
  auto shared_group_completion_block = base::CallbackToBlock(
      base::BindOnce(&TestShareKitService::SetTabGroupCollabIdFromGroupId,
                     weak_pointer_factory_.GetWeakPtr(), tab_group_id));
  viewController.sharedGroupCompletionBlock = shared_group_completion_block;

  UINavigationController* navController = [[UINavigationController alloc]
      initWithRootViewController:viewController];
  [config.baseViewController presentViewController:navController
                                          animated:NO
                                        completion:nil];
  return @"sharedFlow";
}

NSString* TestShareKitService::ManageTabGroup(
    ShareKitManageConfiguration* config) {
  FakeShareKitFlowViewController* viewController =
      [[FakeShareKitFlowViewController alloc]
          initWithType:FakeShareKitFlowType::kManage];
  viewController.completionBlock = config.completion;

  UINavigationController* navController = [[UINavigationController alloc]
      initWithRootViewController:viewController];
  [config.baseViewController presentViewController:navController
                                          animated:NO
                                        completion:nil];
  return @"manageFlow";
}

NSString* TestShareKitService::JoinTabGroup(ShareKitJoinConfiguration* config) {
  is_owner_ = false;
  FakeShareKitFlowViewController* viewController =
      [[FakeShareKitFlowViewController alloc]
          initWithType:FakeShareKitFlowType::kJoin];
  viewController.completionBlock = config.completion;

  // Set the joined group completion block.
  auto joined_group_completion_block = ^(NSString* collab_id) {
    CreateSharedTabGroupInFakeServer(collab_id);
  };

  viewController.sharedGroupCompletionBlock = joined_group_completion_block;

  UINavigationController* navController = [[UINavigationController alloc]
      initWithRootViewController:viewController];
  [config.baseViewController presentViewController:navController
                                          animated:NO
                                        completion:nil];
  return @"joinFlow";
}

UIViewController* TestShareKitService::FacePile(
    ShareKitFacePileConfiguration* config) {
  return [[UIViewController alloc] init];
}

void TestShareKitService::ReadGroups(ShareKitReadConfiguration* config) {
  MemberRole member_role = is_owner_ ? data_sharing_pb::MEMBER_ROLE_OWNER
                                     : data_sharing_pb::MEMBER_ROLE_MEMBER;
  auto callback = config.callback;

  data_sharing_pb::ReadGroupsResult read_result;
  for (ShareKitReadGroupParamConfiguration* inner_config in config
           .groupsParam) {
    *read_result.add_group_data() =
        CreateGroupData(member_role, inner_config.collabID);
  }
  callback(read_result);
}

void TestShareKitService::LeaveGroup(ShareKitLeaveConfiguration* config) {
  ShareKitDeleteConfiguration* deleteConfig =
      [[ShareKitDeleteConfiguration alloc] init];
  deleteConfig.collabID = config.collabID;
  deleteConfig.callback = config.callback;
  DeleteGroup(deleteConfig);
}

void TestShareKitService::DeleteGroup(ShareKitDeleteConfiguration* config) {
  auto callback = config.callback;
  std::optional<tab_groups::SavedTabGroup> tab_group;
  tab_groups::CollaborationId collaboration_id =
      tab_groups::CollaborationId(base::SysNSStringToUTF8(config.collabID));

  for (const auto& group : tab_group_sync_service_->GetAllGroups()) {
    if (group.collaboration_id().has_value() &&
        group.collaboration_id().value() == collaboration_id) {
      tab_group = group;
      break;
    }
  }
  if (!tab_group) {
    callback(absl::Status(absl::StatusCode::kUnknown,
                          "Deleting shared group failed."));
    return;
  }
  callback(absl::Status(absl::StatusCode::kOk, std::string()));

  base::Uuid group_guid = tab_group->saved_guid();
  // Delays group deletion to simulate a server call.
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW, kDeleteGroupDelay.InNanoseconds()),
      dispatch_get_main_queue(), ^{
        chrome_test_util::DeleteSharedGroupFromFakeServer(group_guid);
        chrome_test_util::TriggerSyncCycle(syncer::SAVED_TAB_GROUP);
      });
}

void TestShareKitService::LookupGaiaIdByEmail(
    ShareKitLookupGaiaIDConfiguration* config) {
  // TODO(crbug.com/358373145): add fake implementation.
}

id<ShareKitAvatarPrimitive> TestShareKitService::AvatarImage(
    ShareKitAvatarConfiguration* config) {
  // TODO(crbug.com/375366568): add fake implementation.
  return nil;
}

void TestShareKitService::Shutdown() {
  data_sharing_service_->SetUIDelegate(nullptr);
}

void TestShareKitService::SetTabGroupCollabIdFromGroupId(
    tab_groups::LocalTabGroupID tab_group_id,
    NSString* collab_id) {
  if (tab_group_sync_service_ && collab_id) {
    syncer::CollaborationId collaboration_id(
        base::SysNSStringToUTF8(collab_id));
    // It is necessary to make the collab available on both the sync server and
    // the finder.
    chrome_test_util::AddCollaboration(collaboration_id.value());
    tab_group_sync_service_->GetCollaborationFinderForTesting()
        ->SetCollaborationAvailableForTesting(collaboration_id);

    std::optional<tab_groups::SavedTabGroup> saved_group =
        tab_group_sync_service_->GetGroup(tab_group_id);
    if (saved_group && !saved_group->is_shared_tab_group()) {
      chrome_test_util::AddCollaborationGroupToFakeServer(
          collaboration_id.value());
      chrome_test_util::TriggerSyncCycle(syncer::COLLABORATION_GROUP);
      // TODO(crbug.com/382557489): implement the callback.
      tab_group_sync_service_->MakeTabGroupShared(
          tab_group_id, collaboration_id.value(),
          tab_groups::TabGroupSyncService::TabGroupSharingCallback());
    }
  }
}

void TestShareKitService::SetTabGroupCollabIdFromGroupGuid(
    base::Uuid group_guid,
    NSString* collab_id) {
  std::optional<tab_groups::SavedTabGroup> saved_group =
      tab_group_sync_service_->GetGroup(group_guid);
  if (saved_group && !saved_group->is_shared_tab_group()) {
    // Make the group shared.
    SetTabGroupCollabIdFromGroupId(saved_group->local_group_id().value(),
                                   collab_id);
  }
}

void TestShareKitService::CreateSharedTabGroupInFakeServer(
    NSString* collab_id) {
  CHECK(!is_owner_);
  if (!tab_group_sync_service_) {
    return;
  }

  base::Uuid group_guid = base::Uuid::GenerateRandomV4();
  std::vector<tab_groups::SavedTabGroupTab> tabs;
  tab_groups::SavedTabGroupTab tab = CreateTab(group_guid);
  tabs.push_back(tab);
  chrome_test_util::AddTabToFakeServer(tab);
  chrome_test_util::AddGroupToFakeServer(
      CreateGroup(u"shared group", tabs, group_guid));
  chrome_test_util::TriggerSyncCycle(syncer::SAVED_TAB_GROUP);

  // Post delayed task in order to make sure that `NotifyTabGroupAdded` is
  // called first.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&TestShareKitService::SetTabGroupCollabIdFromGroupGuid,
                     weak_pointer_factory_.GetWeakPtr(), group_guid, collab_id),
      base::Milliseconds(1000));
}
