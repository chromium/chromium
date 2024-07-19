// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_MODEL_IOS_TAB_GROUP_SYNC_UTIL_H_
#define IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_MODEL_IOS_TAB_GROUP_SYNC_UTIL_H_

#import "components/saved_tab_groups/types.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/web_state.h"

namespace tab_groups {
class SavedTabGroup;
class TabGroupSyncService;
}  // namespace tab_groups

class Browser;
class BrowserList;

namespace tab_groups {
namespace utils {

// Struct containing local tab group information.
struct LocalTabGroupInfo {
  const TabGroup* tab_group = nil;
  WebStateList* web_state_list = nil;
  Browser* browser = nil;
};

// Struct containing local tab information.
struct LocalTabInfo {
  const TabGroup* tab_group = nil;
  int index_in_group = WebStateList::kInvalidIndex;
};

// Find the `tab_group` and the `web_state_list` corresponding to
// the given `saved_tab_group`.
LocalTabGroupInfo GetLocalTabGroupInfo(
    BrowserList* browser_list,
    const tab_groups::SavedTabGroup& saved_tab_group);

// Find the `tab_group` and the `web_state_list` corresponding to
// the given `tab_group_id`.
LocalTabGroupInfo GetLocalTabGroupInfo(
    BrowserList* browser_list,
    const tab_groups::LocalTabGroupID& tab_group_id);

// Find the `tab_group` and the `tab_index` corresponding to
// the given `web_state_identifier` into regular browsers.
LocalTabInfo GetLocalTabInfo(BrowserList* browser_list,
                             web::WebStateID web_state_identifier);

// Find the `tab_group` and the `tab_index` corresponding to
// the given `web_state_identifier`.
LocalTabInfo GetLocalTabInfo(WebStateList* web_state_list,
                             web::WebStateID web_state_identifier);

// Removes the association between the local tab group mapping and the
// `tab_group`. All tabs within the tab_group are closed.
void CloseTabGroupLocally(const TabGroup* tab_group,
                          WebStateList* web_state_list,
                          TabGroupSyncService* sync_service);

// Moves tab group to the `destination_tab_group_index` in
// `destination_browser`. It is an error to try to move a tab across profiles
// (incognito <-> regular).
void MoveTabGroupToBrowser(const TabGroup* tab_group,
                           Browser* destination_browser,
                           int destination_tab_group_index);

}  // namespace utils
}  // namespace tab_groups

#endif  // IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_MODEL_IOS_TAB_GROUP_SYNC_UTIL_H_
