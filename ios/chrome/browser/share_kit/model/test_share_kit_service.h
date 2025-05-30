// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARE_KIT_MODEL_TEST_SHARE_KIT_SERVICE_H_
#define IOS_CHROME_BROWSER_SHARE_KIT_MODEL_TEST_SHARE_KIT_SERVICE_H_

#import <UIKit/UIKit.h>

#import <map>

#import "base/ios/block_types.h"
#import "base/memory/weak_ptr.h"
#import "components/data_sharing/public/protocol/group_data.pb.h"
#import "components/saved_tab_groups/public/tab_group_sync_service.h"
#import "components/saved_tab_groups/public/types.h"
#import "ios/chrome/browser/share_kit/model/share_kit_service.h"

class TabGroupService;

namespace collaboration {
class CollaborationService;
}  // namespace collaboration

namespace data_sharing {
class DataSharingService;
}  // namespace data_sharing

// Test implementation of the ShareKitService.
class TestShareKitService : public ShareKitService {
 public:
  TestShareKitService(
      data_sharing::DataSharingService* data_sharing_service,
      collaboration::CollaborationService* collaboration_service,
      tab_groups::TabGroupSyncService* tab_group_sync_service,
      TabGroupService* tab_group_service);
  TestShareKitService(const TestShareKitService&) = delete;
  TestShareKitService& operator=(const TestShareKitService&) = delete;
  ~TestShareKitService() override;

  // ShareKitService.
  bool IsSupported() const override;
  void PrimaryAccountChanged() override;
  void CancelSession(NSString* session_id) override;
  NSString* ShareTabGroup(ShareKitShareGroupConfiguration* config) override;
  NSString* ManageTabGroup(ShareKitManageConfiguration* config) override;
  NSString* JoinTabGroup(ShareKitJoinConfiguration* config) override;
  UIView* FacePileView(ShareKitFacePileConfiguration* config) override;
  void ReadGroups(ShareKitReadGroupsConfiguration* config) override;
  void ReadGroupWithToken(
      ShareKitReadGroupWithTokenConfiguration* config) override;
  void LeaveGroup(ShareKitLeaveConfiguration* config) override;
  void DeleteGroup(ShareKitDeleteConfiguration* config) override;
  void LookupGaiaIdByEmail(ShareKitLookupGaiaIDConfiguration* config) override;
  id<ShareKitAvatarPrimitive> AvatarImage(
      ShareKitAvatarConfiguration* config) override;

  // Creates the shared tab group with the given `collab_id` and marks the
  // identity as `owner` of it and saves it to the fake server.
  void CreateSharedTabGroupInFakeServer(bool owner, NSString* collab_id);

  // KeyedService.
  void Shutdown() override;

 private:
  // Shares the group with `tab_group_id` to the `collab_id`.
  // `continuation_block` will be called once the group is ready to be shared.
  void ShareGroup(tab_groups::LocalTabGroupID tab_group_id,
                  NSString* collab_id,
                  ProceduralBlock continuation_block);

  // Prepares to share the group with `tab_group_id` to the `collab_id` as
  // `owner`. This is updating the fake sync server.
  void PrepareToShareGroup(bool owner,
                           tab_groups::LocalTabGroupID tab_group_id,
                           NSString* collab_id);

  // Sets the `collab_id` for the given `tab_group_id` and sets the user as
  // `owner`.
  void SetTabGroupCollabIdFromGroupId(bool owner,
                                      tab_groups::LocalTabGroupID tab_group_id,
                                      NSString* collab_id);

  // Sets the `collab_id` for the given `group_guid` and sets the user as
  // `owner`.
  void SetTabGroupCollabIdFromGroupGuid(bool owner,
                                        base::Uuid group_guid,
                                        NSString* collab_id);

  // Used as a callback called when a group `saved_group_guid` is shared.
  // `result` represents if sharing a group succeeded.
  void ProcessTabGroupSharingResult(
      base::Uuid saved_group_guid,
      tab_groups::TabGroupSyncService::TabGroupSharingResult result);

  // Map containing the role of `foo1` identity for each group, based on its
  // collaboration id.
  std::map<std::string, data_sharing_pb::MemberRole> group_to_membership_;

  raw_ptr<data_sharing::DataSharingService> data_sharing_service_;
  raw_ptr<tab_groups::TabGroupSyncService> tab_group_sync_service_;
  raw_ptr<TabGroupService> tab_group_service_;

  // The set of group ID that is being shared.
  std::set<base::Uuid> processing_group_guids_;

  // The currently presented view controller for the latest flow.
  __weak UIViewController* presented_view_controller_;

  base::WeakPtrFactory<TestShareKitService> weak_pointer_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_SHARE_KIT_MODEL_TEST_SHARE_KIT_SERVICE_H_
