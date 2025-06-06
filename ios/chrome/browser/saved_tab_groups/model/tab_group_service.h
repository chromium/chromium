// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_MODEL_TAB_GROUP_SERVICE_H_
#define IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_MODEL_TAB_GROUP_SERVICE_H_

#import <map>

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/saved_tab_groups/public/types.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_groups_delegate.h"

class ProfileIOS;
class ShareKitService;
class TabGroup;

namespace collaboration {
class IOSCollaborationControllerDelegate;
}

namespace tab_groups {
class TabGroupSyncService;
}  // namespace tab_groups

namespace web {
class WebState;
}  // namespace web

// Service that manages all tab group-related functions.
class TabGroupService : public KeyedService, public WebStateListGroupsDelegate {
 public:
  explicit TabGroupService(
      ProfileIOS* profile,
      tab_groups::TabGroupSyncService* tab_group_sync_service);
  ~TabGroupService() override;

  // KeyedService implementation:
  void Shutdown() override;

  // WebStateListGroupsDelegate implementation:
  bool ShouldDeleteGroup(const TabGroup* group) override;
  std::unique_ptr<web::WebState> WebStateToAddToEmptyGroup() override;

  // Registers a collaboration controller delegate for future access. Not all
  // delegates will be registered. It is an error to register two delegate for
  // the same group.
  void RegisterCollaborationControllerDelegate(
      tab_groups::LocalTabGroupID tab_group_id,
      base::WeakPtr<collaboration::IOSCollaborationControllerDelegate>
          controller_delegate);

  // Unregisters a collaboration controller delegate. Registered delegate should
  // call this at the end of their flow.
  void UnregisterCollaborationControllerDelegate(
      tab_groups::LocalTabGroupID tab_group_id);

  // Returns the registered collaboration controller delegate, if any.
  collaboration::IOSCollaborationControllerDelegate* GetDelegateForGroup(
      tab_groups::LocalTabGroupID tab_group_id);

  // YES if the group only have 1 tab and the group is shared.
  bool ShouldDisplayLastTabCloseAlert(const TabGroup* group);

  // true if the group is shared.
  bool IsSharedGroup(const TabGroup* group);

 private:
  // Associated profile.
  raw_ptr<ProfileIOS> profile_;

  // The service to handle tab group sync.
  raw_ptr<tab_groups::TabGroupSyncService> tab_group_sync_service_;

  // The share kit service.
  raw_ptr<ShareKitService> share_kit_service_;

  // The map holding the collaboration controller delegate with their associated
  // tab group id.
  std::map<tab_groups::LocalTabGroupID,
           base::WeakPtr<collaboration::IOSCollaborationControllerDelegate>>
      group_to_controller_delegate_;
};

#endif  // IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_MODEL_TAB_GROUP_SERVICE_H_
