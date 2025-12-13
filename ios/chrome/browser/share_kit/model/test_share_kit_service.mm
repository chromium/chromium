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
#import "ios/chrome/browser/collaboration/model/ios_collaboration_controller_delegate.h"
#import "ios/chrome/browser/data_sharing/model/data_sharing_sdk_delegate_ios.h"
#import "ios/chrome/browser/data_sharing/model/data_sharing_ui_delegate_ios.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_service.h"
#import "ios/chrome/browser/share_kit/model/fake_share_kit_flow_view_controller.h"
#import "ios/chrome/browser/share_kit/model/share_kit_delete_configuration.h"
#import "ios/chrome/browser/share_kit/model/share_kit_join_configuration.h"
#import "ios/chrome/browser/share_kit/model/share_kit_leave_configuration.h"
#import "ios/chrome/browser/share_kit/model/share_kit_manage_configuration.h"
#import "ios/chrome/browser/share_kit/model/share_kit_read_configuration.h"
#import "ios/chrome/browser/share_kit/model/share_kit_share_group_configuration.h"
#import "ios/chrome/browser/share_kit/model/test_constants.h"
#import "ios/chrome/browser/share_kit/model/test_share_kit_avatar_primitive.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/test/app/sync_test_util.h"

using data_sharing::GroupId;
using data_sharing_pb::GroupData;
using data_sharing_pb::GroupMember;
using data_sharing_pb::MemberRole;

namespace {

// URL used to create SavedTabGroupTab.
constexpr char kTabURL[] = "https://google.com";

// Title for the shared tab.
constexpr char16_t kSharedTabTitle[] = u"Google";

// Delay to observe when deleting a shared tab group from the server.
constexpr base::TimeDelta kDeleteGroupDelay = base::Seconds(0.5);

// Creates a saved tab belonging to `group_guid` group.
tab_groups::SavedTabGroupTab CreateTab(const base::Uuid& group_guid,
                                       const GURL& url) {
  tab_groups::SavedTabGroupTab saved_tab(url, kSharedTabTitle, group_guid,
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
data_sharing_pb::GroupMember CreateGroupMember(const GaiaId& gaia_id,
                                               MemberRole member_role) {
  GroupMember member;
  if (member_role == data_sharing_pb::MEMBER_ROLE_OWNER) {
    member.set_display_name("Owner");
    member.set_email("owner@mail.com");
  } else {
    member.set_display_name("Member");
    member.set_email("member@mail.com");
  }
  member.set_gaia_id(gaia_id.ToString());
  member.set_avatar_url("chrome://newtab");
  member.set_given_name("Given Name");
  member.set_role(member_role);
  return member;
}

// Creates group data for a group containing two users.
// The first user has the given `member_role`.
GroupData CreateGroupData(MemberRole member_role,
                          const std::string& collaboration_id) {
  data_sharing_pb::GroupData group_data;
  group_data.set_group_id(collaboration_id);
  group_data.set_display_name("Display Name");
  *group_data.add_members() =
      CreateGroupMember([FakeSystemIdentity fakeIdentity1].gaiaId, member_role);
  MemberRole member_role2 = member_role == data_sharing_pb::MEMBER_ROLE_OWNER
                                ? data_sharing_pb::MEMBER_ROLE_MEMBER
                                : data_sharing_pb::MEMBER_ROLE_OWNER;
  *group_data.add_members() = CreateGroupMember(
      [FakeSystemIdentity fakeIdentity2].gaiaId, member_role2);
  *group_data.add_members() =
      CreateGroupMember([FakeSystemIdentity fakeIdentity3].gaiaId,
                        data_sharing_pb::MEMBER_ROLE_MEMBER);
  group_data.set_access_token("fake_access_token");
  return group_data;
}

}  // namespace

TestShareKitService::TestShareKitService(
    data_sharing::DataSharingService* data_sharing_service,
    collaboration::CollaborationService* collaboration_service,
    tab_groups::TabGroupSyncService* tab_group_sync_service,
    TabGroupService* tab_group_service)
    : data_sharing_service_(data_sharing_service),
      tab_group_sync_service_(tab_group_sync_service),
      tab_group_service_(tab_group_service) {
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

void TestShareKitService::CancelSession(NSString* session_id) {
  [presented_view_controller_.presentingViewController
      dismissViewControllerAnimated:NO
                         completion:nil];
}

NSString* TestShareKitService::ShareTabGroup(
    ShareKitShareGroupConfiguration* config) {
  const TabGroup* tab_group = config.tabGroup;
  if (!tab_group) {
    return nil;
  }
  tab_groups::LocalTabGroupID tab_group_id = tab_group->tab_group_id();

  FakeShareKitFlowViewController* viewController =
      [[FakeShareKitFlowViewController alloc]
          initWithType:FakeShareKitFlowType::kShare];
  viewController.flowCompleteBlock = config.completion;

  // Set the shared group completion block.
  auto shared_group_completion_block = base::CallbackToBlock(
      base::BindOnce(&TestShareKitService::ShareGroup,
                     weak_pointer_factory_.GetWeakPtr(), tab_group_id));
  viewController.actionAcceptedBlock = shared_group_completion_block;

  UINavigationController* navController = [[UINavigationController alloc]
      initWithRootViewController:viewController];
  [config.baseViewController presentViewController:navController
                                          animated:NO
                                        completion:nil];
  // Keep a weak link to potentially dismiss it.
  presented_view_controller_ = navController;
  return @"sharedFlow";
}

NSString* TestShareKitService::ManageTabGroup(
    ShareKitManageConfiguration* config) {
  FakeShareKitFlowViewController* viewController =
      [[FakeShareKitFlowViewController alloc]
          initWithType:FakeShareKitFlowType::kManage];
  viewController.flowCompleteBlock = config.completion;

  UINavigationController* navController = [[UINavigationController alloc]
      initWithRootViewController:viewController];
  [config.baseViewController presentViewController:navController
                                          animated:NO
                                        completion:nil];
  // Keep a weak link to potentially dismiss it.
  presented_view_controller_ = navController;
  return @"manageFlow";
}

NSString* TestShareKitService::JoinTabGroup(ShareKitJoinConfiguration* config) {
  FakeShareKitFlowViewController* viewController =
      [[FakeShareKitFlowViewController alloc]
          initWithType:FakeShareKitFlowType::kJoin];
  viewController.flowCompleteBlock = config.completion;

  // Set the joined group completion block.
  auto joined_group_completion_block = ^(NSString* collab_id,
                                         ProceduralBlock continuation_block) {
    CreateSharedTabGroupInFakeServer(/*owner=*/false, collab_id, GURL(kTabURL));
    continuation_block();
  };

  viewController.actionAcceptedBlock = joined_group_completion_block;

  UINavigationController* navController = [[UINavigationController alloc]
      initWithRootViewController:viewController];
  [config.baseViewController presentViewController:navController
                                          animated:NO
                                        completion:nil];
  // Keep a weak link to potentially dismiss it.
  presented_view_controller_ = navController;
  return @"joinFlow";
}

void TestShareKitService::ReadGroups(ShareKitReadGroupsConfiguration* config) {
  auto callback = config.callback;

  data_sharing_pb::ReadGroupsResult read_result;
  for (ShareKitReadGroupParamConfiguration* inner_config in config
           .groupsParam) {
    MemberRole member_role = data_sharing_pb::MEMBER_ROLE_UNSPECIFIED;
    std::string collab_id = base::SysNSStringToUTF8(inner_config.collabID);
    if (group_to_membership_.contains(collab_id)) {
      member_role = group_to_membership_[collab_id];
    }
    *read_result.add_group_data() = CreateGroupData(member_role, collab_id);
  }
  callback(read_result);
}

void TestShareKitService::ReadGroupWithToken(
    ShareKitReadGroupWithTokenConfiguration* config) {
  MemberRole member_role = data_sharing_pb::MEMBER_ROLE_UNSPECIFIED;
  std::string collab_id = base::SysNSStringToUTF8(config.collabID);
  if (group_to_membership_.contains(collab_id)) {
    member_role = group_to_membership_[collab_id];
  }
  auto callback = config.callback;

  data_sharing_pb::ReadGroupsResult read_result;
  *read_result.add_group_data() = CreateGroupData(member_role, collab_id);
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
  syncer::CollaborationId collaboration_id =
      syncer::CollaborationId(base::SysNSStringToUTF8(config.collabID));

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
  return [[TestShareKitAvatarPrimitive alloc] init];
}

void TestShareKitService::Shutdown() {
  data_sharing_service_->SetUIDelegate(nullptr);
}

void TestShareKitService::ShareGroup(tab_groups::LocalTabGroupID tab_group_id,
                                     NSString* collab_id,
                                     ProceduralBlock continuation_block) {
  auto shared_url_block = ^(GURL _) {
    continuation_block();
  };
  PrepareToShareGroup(/*owner=*/true, tab_group_id, collab_id);
  tab_group_service_->GetDelegateForGroup(tab_group_id)
      ->ShareGroupAndGenerateLink(base::SysNSStringToUTF8(collab_id), "token",
                                  base::BindOnce(shared_url_block));
}

void TestShareKitService::PrepareToShareGroup(
    bool owner,
    tab_groups::LocalTabGroupID tab_group_id,
    NSString* collab_id) {
  if (!tab_group_sync_service_ || !collab_id) {
    return;
  }

  group_to_membership_[base::SysNSStringToUTF8(collab_id)] =
      owner ? data_sharing_pb::MEMBER_ROLE_OWNER
            : data_sharing_pb::MEMBER_ROLE_MEMBER;

  syncer::CollaborationId collaboration_id(base::SysNSStringToUTF8(collab_id));
  // It is necessary to make the collab available on both the sync server and
  // the finder.
  chrome_test_util::AddCollaboration(collaboration_id);
  tab_group_sync_service_->GetCollaborationFinderForTesting()
      ->SetCollaborationAvailableForTesting(collaboration_id);

  std::optional<tab_groups::SavedTabGroup> saved_group =
      tab_group_sync_service_->GetGroup(tab_group_id);
  if (saved_group && !saved_group->is_shared_tab_group()) {
    chrome_test_util::AddCollaborationGroupToFakeServer(collaboration_id);
    chrome_test_util::TriggerSyncCycle(syncer::COLLABORATION_GROUP);
  }
}

void TestShareKitService::SetTabGroupCollabIdFromGroupId(
    bool owner,
    tab_groups::LocalTabGroupID tab_group_id,
    NSString* collab_id) {
  if (!tab_group_sync_service_ || !collab_id) {
    return;
  }

  PrepareToShareGroup(owner, tab_group_id, collab_id);

  std::optional<tab_groups::SavedTabGroup> saved_group =
      tab_group_sync_service_->GetGroup(tab_group_id);
  if (saved_group && !saved_group->is_shared_tab_group()) {
    tab_group_sync_service_->MakeTabGroupShared(
        tab_group_id,
        syncer::CollaborationId(base::SysNSStringToUTF8(collab_id)),
        base::BindOnce(&TestShareKitService::ProcessTabGroupSharingResult,
                       weak_pointer_factory_.GetWeakPtr(),
                       saved_group.value().saved_guid()));
  }
}

void TestShareKitService::SetTabGroupCollabIdFromGroupGuid(
    bool owner,
    base::Uuid group_guid,
    NSString* collab_id) {
  std::optional<tab_groups::SavedTabGroup> saved_group =
      tab_group_sync_service_->GetGroup(group_guid);
  if (saved_group && !saved_group->is_shared_tab_group()) {
    // Make the group shared.
    SetTabGroupCollabIdFromGroupId(owner, saved_group->local_group_id().value(),
                                   collab_id);
  }
}

void TestShareKitService::ProcessTabGroupSharingResult(
    base::Uuid saved_group_guid,
    tab_groups::TabGroupSyncService::TabGroupSharingResult result) {
  if (result !=
      tab_groups::TabGroupSyncService::TabGroupSharingResult::kSuccess) {
    return;
  }

  processing_group_guids_.erase(saved_group_guid);
  if (processing_group_guids_.size() > 0) {
    // If the process to share a group is still on-going, do not delete
    // entities.
    return;
  }

  // Delete `SAVED_TAB_GROUP` entities after all groups are successfully shared
  // and keep only `SHARED_TAB_GROUP_DATA` entities in order to fake the
  // behavior of sharing a group across different accounts. The fake server
  // doesn't support multiple accounts and a member in a shared group shouldn't
  // receieve the `SAVED_TAB_GROUP` data.
  chrome_test_util::DeleteAllEntitiesForDataType(syncer::SAVED_TAB_GROUP);
}

void TestShareKitService::CreateSharedTabGroupInFakeServer(bool owner,
                                                           NSString* collab_id,
                                                           const GURL& url) {
  if (!tab_group_sync_service_) {
    return;
  }

  base::Uuid group_guid = base::Uuid::GenerateRandomV4();
  std::vector<tab_groups::SavedTabGroupTab> tabs;
  tab_groups::SavedTabGroupTab tab = CreateTab(group_guid, url);
  tabs.push_back(tab);
  chrome_test_util::AddTabToFakeServer(tab);
  chrome_test_util::AddGroupToFakeServer(
      CreateGroup(u"shared group", tabs, group_guid));
  chrome_test_util::TriggerSyncCycle(syncer::SAVED_TAB_GROUP);

  // Add `group_guid` to keep the track of which group is being shared.
  processing_group_guids_.insert(group_guid);

  // Post delayed task in order to make sure that `NotifyTabGroupAdded` is
  // called first.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&TestShareKitService::SetTabGroupCollabIdFromGroupGuid,
                     weak_pointer_factory_.GetWeakPtr(), owner, group_guid,
                     collab_id),
      base::Milliseconds(1000));
}
