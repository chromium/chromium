// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_MODEL_IOS_TAB_GROUP_SYNC_UTIL_H_
#define IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_MODEL_IOS_TAB_GROUP_SYNC_UTIL_H_

#import "components/saved_tab_groups/types.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"

namespace tab_groups {
class SavedTabGroup;
}  // namespace tab_groups

class Browser;
class BrowserList;

namespace tab_groups {
namespace utils {

// Struct that containing local tab group information.
struct LocalTabGroupInfo {
  const TabGroup* tab_group = nil;
  WebStateList* web_state_list = nil;
  Browser* browser = nil;
};

// Find the `tab_group` and the `web_state_list` corresponding to
// the `saved_tab_group`.
LocalTabGroupInfo GetLocalTabGroupInfo(
    BrowserList* browser_list,
    const tab_groups::SavedTabGroup& saved_tab_group);

// Find the `tab_group` and the `web_state_list` corresponding to
// the `tab_group_id`.
LocalTabGroupInfo GetLocalTabGroupInfo(
    BrowserList* browser_list,
    const tab_groups::LocalTabGroupID& tab_group_id);

}  // namespace utils
}  // namespace tab_groups

#endif  // IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_MODEL_IOS_TAB_GROUP_SYNC_UTIL_H_
