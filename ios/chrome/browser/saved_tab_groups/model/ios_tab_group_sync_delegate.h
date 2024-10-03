// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_MODEL_IOS_TAB_GROUP_SYNC_DELEGATE_H_
#define IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_MODEL_IOS_TAB_GROUP_SYNC_DELEGATE_H_

#import <vector>

#import "base/memory/raw_ptr.h"
#import "base/no_destructor.h"
#import "components/saved_tab_groups/delegate/tab_group_sync_delegate.h"
#import "components/saved_tab_groups/public/saved_tab_group.h"
#import "components/saved_tab_groups/public/types.h"

class Browser;
class BrowserList;
class TabGroup;
class TabInsertionBrowserAgent;

namespace tab_groups {
class TabGroupLocalUpdateObserver;
class TabGroupSyncService;

namespace utils {
struct LocalTabGroupInfo;
}  // namespace utils

}  // namespace tab_groups

namespace web {
class WebState;
}  // namespace web

namespace tab_groups {

// IOS Subclass of the TabGroupSyncDelegate.
class IOSTabGroupSyncDelegate : public TabGroupSyncDelegate {
 public:
  IOSTabGroupSyncDelegate(
      BrowserList* browser_list,
      TabGroupSyncService* sync_service,
      std::unique_ptr<TabGroupLocalUpdateObserver> local_update_observer);

  IOSTabGroupSyncDelegate(const IOSTabGroupSyncDelegate&) = delete;
  IOSTabGroupSyncDelegate& operator=(const IOSTabGroupSyncDelegate&) = delete;
  ~IOSTabGroupSyncDelegate() override;

  // TabGroupSyncDelegate.
  void HandleOpenTabGroupRequest(
      const base::Uuid& sync_tab_group_id,
      std::unique_ptr<TabGroupActionContext> context) override;
  std::unique_ptr<ScopedLocalObservationPauser>
  CreateScopedLocalObserverPauser() override;
  void CreateLocalTabGroup(const SavedTabGroup& saved_tab_group) override;
  void CloseLocalTabGroup(const LocalTabGroupID& local_tab_group_id) override;
  void DisconnectLocalTabGroup(const LocalTabGroupID& local_id) override;
  void UpdateLocalTabGroup(const SavedTabGroup& saved_tab_group) override;
  std::vector<LocalTabGroupID> GetLocalTabGroupIds() override;
  std::vector<LocalTabID> GetLocalTabIdsForTabGroup(
      const LocalTabGroupID& local_tab_group_id) override;
  void CreateRemoteTabGroup(const LocalTabGroupID& local_tab_group_id) override;

 private:
  // Retrieves the browser associated with the scene with the highest level of
  // activation.
  Browser* GetMostActiveSceneBrowser();

  // Inserts the `distant_tab` using `tab_insertion_browser_agent` at
  // `web_state_index`.
  web::WebState* InsertDistantTab(
      const SavedTabGroupTab& tab,
      TabInsertionBrowserAgent* tab_insertion_browser_agent,
      int web_state_index,
      const TabGroup* tab_group);

  // Updates the given `web_state` to match the distant `synced_tab`.
  void UpdateLocalWebState(int web_state_index,
                           web::WebState* web_state,
                           tab_groups::utils::LocalTabGroupInfo tab_group_info,
                           const SavedTabGroupTab& synced_tab);

  // Updates the association of the local tab id on the server.
  void UpdateLocalTabId(web::WebState* web_state,
                        const TabGroup* tab_group,
                        const SavedTabGroupTab& saved_tab);

  // Updates the visual data of the local tab group to match the
  // `SavedTabGroup`.
  void UpdateLocalGroupVisualData(utils::LocalTabGroupInfo tab_group_info,
                                  const SavedTabGroup& saved_tab_group);

  // Creates a local tab group based on `saved_tab_group` and `browser`. Pass
  // nullptr for the browser to create the group on the most active window.
  // Returns the ID used to create the new group.
  std::optional<LocalTabGroupID> CreateLocalTabGroupImpl(
      const SavedTabGroup& saved_tab_group,
      Browser* browser);

  raw_ptr<BrowserList> browser_list_ = nullptr;
  raw_ptr<TabGroupSyncService> sync_service_ = nullptr;
  std::unique_ptr<TabGroupLocalUpdateObserver> local_update_observer_;
};

}  // namespace tab_groups

#endif  // IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_MODEL_IOS_TAB_GROUP_SYNC_DELEGATE_H_
