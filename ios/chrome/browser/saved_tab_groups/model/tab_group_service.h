// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_MODEL_TAB_GROUP_SERVICE_H_
#define IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_MODEL_TAB_GROUP_SERVICE_H_

#import "base/memory/raw_ptr.h"
#import "components/keyed_service/core/keyed_service.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_groups_delegate.h"

class ProfileIOS;
class ShareKitService;
class TabGroup;

namespace tab_groups {
class TabGroupSyncService;
}  // namespace tab_groups

namespace web {
class WebState;
}  // namespace web

// Service that manages models required to support
class TabGroupService : public KeyedService, public WebStateListGroupsDelegate {
 public:
  explicit TabGroupService(
      ProfileIOS* profile,
      tab_groups::TabGroupSyncService* tab_group_sync_service,
      ShareKitService* share_kit_service);
  ~TabGroupService() override;

  // KeyedService implementation:
  void Shutdown() override;

  // WebStateListGroupsDelegate implementation:
  bool ShouldDeleteGroup(const TabGroup* group) override;
  std::unique_ptr<web::WebState> WebStateToAddToEmptyGroup() override;

 private:
  // true if the group is shared.
  bool IsSharedGroup(const TabGroup* group);

  // Associated profile.
  raw_ptr<ProfileIOS> profile_;

  // The service to handle tab group sync.
  raw_ptr<tab_groups::TabGroupSyncService> tab_group_sync_service_;

  // The share kit service.
  raw_ptr<ShareKitService> share_kit_service_;
};

#endif  // IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_MODEL_TAB_GROUP_SERVICE_H_
