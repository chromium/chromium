// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARE_KIT_MODEL_TEST_SHARE_KIT_SERVICE_H_
#define IOS_CHROME_BROWSER_SHARE_KIT_MODEL_TEST_SHARE_KIT_SERVICE_H_

#import <UIKit/UIKit.h>

#import "base/memory/weak_ptr.h"
#import "components/saved_tab_groups/public/types.h"
#import "ios/chrome/browser/share_kit/model/share_kit_service.h"

namespace collaboration {
class CollaborationService;
}  // namespace collaboration

namespace data_sharing {
class DataSharingService;
}  // namespace data_sharing

namespace tab_groups {
class TabGroupSyncService;
}  // namespace tab_groups

// Test implementation of the ShareKitService.
class TestShareKitService : public ShareKitService {
 public:
  TestShareKitService(
      data_sharing::DataSharingService* data_sharing_service,
      collaboration::CollaborationService* collaboration_service,
      tab_groups::TabGroupSyncService* tab_group_sync_service);
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
  UIViewController* FacePile(ShareKitFacePileConfiguration* config) override;
  void ReadGroups(ShareKitReadConfiguration* config) override;
  void LeaveGroup(ShareKitLeaveConfiguration* config) override;
  void DeleteGroup(ShareKitDeleteConfiguration* config) override;
  void LookupGaiaIdByEmail(ShareKitLookupGaiaIDConfiguration* config) override;
  id<ShareKitAvatarPrimitive> AvatarImage(
      ShareKitAvatarConfiguration* config) override;

  // Creates the shared tab group with the given `collab_id` and saves it to the
  // fake server.
  void CreateSharedTabGroupInFakeServer(NSString* collab_id);

  // KeyedService.
  void Shutdown() override;

 private:
  // Sets the `collab_id` for the given `tab_group_id`.
  void SetTabGroupCollabIdFromGroupId(tab_groups::LocalTabGroupID tab_group_id,
                                      NSString* collab_id);

  // Sets the `collab_id` for the given `group_guid`.
  void SetTabGroupCollabIdFromGroupGuid(base::Uuid group_guid,
                                        NSString* collab_id);

  // Whether the user is the owner of the most-recently-updated group. This is
  // a workaround to update the user's role based on the specific tested flow.
  bool is_owner_ = false;

  raw_ptr<data_sharing::DataSharingService> data_sharing_service_;
  raw_ptr<tab_groups::TabGroupSyncService> tab_group_sync_service_;
  base::WeakPtrFactory<TestShareKitService> weak_pointer_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_SHARE_KIT_MODEL_TEST_SHARE_KIT_SERVICE_H_
