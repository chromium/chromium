// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/saved_tab_groups/model/ios_tab_group_sync_util.h"

#import "components/saved_tab_groups/saved_tab_group.h"
#import "components/saved_tab_groups/saved_tab_group_tab.h"
#import "components/saved_tab_groups/tab_group_sync_delegate.h"
#import "components/saved_tab_groups/types.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"

using tab_groups::SavedTabGroupTab;

namespace tab_groups {
namespace utils {

LocalTabGroupInfo GetLocalTabGroupInfo(
    ChromeBrowserState* browser_state,
    const tab_groups::SavedTabGroup& saved_tab_group) {
  if (!saved_tab_group.local_group_id().has_value() ||
      saved_tab_group.saved_tabs().size() == 0) {
    return LocalTabGroupInfo{};
  }

  return GetLocalTabGroupInfo(browser_state,
                              saved_tab_group.local_group_id().value());
}

LocalTabGroupInfo GetLocalTabGroupInfo(
    ChromeBrowserState* browser_state,
    const tab_groups::LocalTabGroupID& tab_group_id) {
  BrowserList* browser_list =
      BrowserListFactory::GetForBrowserState(browser_state);
  for (Browser* browser : browser_list->AllRegularBrowsers()) {
    WebStateList* web_state_list = browser->GetWebStateList();
    for (const TabGroup* group : web_state_list->GetGroups()) {
      if (group->tab_group_id() == tab_group_id) {
        return LocalTabGroupInfo{
            .tab_group = group,
            .web_state_list = web_state_list,
            .browser = browser,
        };
      }
    }
  }
  return LocalTabGroupInfo{};
}

}  // namespace utils
}  // namespace tab_groups
