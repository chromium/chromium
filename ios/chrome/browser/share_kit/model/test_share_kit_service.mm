// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/share_kit/model/test_share_kit_service.h"

#import "base/functional/callback_helpers.h"
#import "base/strings/sys_string_conversions.h"
#import "components/data_sharing/public/data_sharing_service.h"
#import "components/saved_tab_groups/public/collaboration_finder.h"
#import "components/saved_tab_groups/public/saved_tab_group.h"
#import "components/saved_tab_groups/public/tab_group_sync_service.h"
#import "ios/chrome/browser/data_sharing/model/data_sharing_ui_delegate_ios.h"
#import "ios/chrome/browser/share_kit/model/fake_share_kit_flow_view_controller.h"
#import "ios/chrome/browser/share_kit/model/share_kit_join_configuration.h"
#import "ios/chrome/browser/share_kit/model/share_kit_manage_configuration.h"
#import "ios/chrome/browser/share_kit/model/share_kit_share_group_configuration.h"
#import "ios/chrome/browser/share_kit/model/test_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/test/app/sync_test_util.h"

TestShareKitService::TestShareKitService(
    data_sharing::DataSharingService* data_sharing_service,
    collaboration::CollaborationService* collaboration_service,
    tab_groups::TabGroupSyncService* sync_service)
    : data_sharing_service_(data_sharing_service), sync_service_(sync_service) {
  if (data_sharing_service_) {
    std::unique_ptr<data_sharing::DataSharingUIDelegateIOS> ui_delegate =
        std::make_unique<data_sharing::DataSharingUIDelegateIOS>(
            this, collaboration_service);
    data_sharing_service_->SetUIDelegate(std::move(ui_delegate));
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
  tab_groups::LocalTabGroupID tab_group_id = tab_group->tab_group_id();

  FakeShareKitFlowViewController* viewController =
      [[FakeShareKitFlowViewController alloc]
          initWithType:FakeShareKitFlowType::kShare];
  viewController.completionBlock = config.completionBlock;

  // Set the shared group completion block.
  auto shared_group_completion_block = base::CallbackToBlock(
      base::BindOnce(&TestShareKitService::SetTabGroupCollabID,
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

  UINavigationController* navController = [[UINavigationController alloc]
      initWithRootViewController:viewController];
  [config.baseViewController presentViewController:navController
                                          animated:NO
                                        completion:nil];
  return @"manageFlow";
}

NSString* TestShareKitService::JoinTabGroup(ShareKitJoinConfiguration* config) {
  FakeShareKitFlowViewController* viewController =
      [[FakeShareKitFlowViewController alloc]
          initWithType:FakeShareKitFlowType::kJoin];
  viewController.completionBlock = config.completionBlock;

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
  // TODO(crbug.com/358373145): add fake implementation.
}

void TestShareKitService::LeaveGroup(ShareKitLeaveConfiguration* config) {
  // TODO(crbug.com/358373145): add fake implementation.
}

void TestShareKitService::DeleteGroup(ShareKitDeleteConfiguration* config) {
  // TODO(crbug.com/358373145): add fake implementation.
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

void TestShareKitService::SetTabGroupCollabID(
    tab_groups::LocalTabGroupID tab_group_id,
    NSString* collab_id) {
  if (sync_service_ && collab_id) {
    std::string collaboration_id = base::SysNSStringToUTF8(collab_id);
    // It is necessary to make the collab available on both the sync server and
    // the finder.
    chrome_test_util::AddCollaboration(collaboration_id);
    sync_service_->GetCollaborationFinderForTesting()
        ->SetCollaborationAvailableForTesting(collaboration_id);

    std::optional<tab_groups::SavedTabGroup> saved_group =
        sync_service_->GetGroup(tab_group_id);
    if (saved_group && !saved_group->is_shared_tab_group()) {
      sync_service_->MakeTabGroupShared(tab_group_id, collaboration_id);
    }
  }
}
