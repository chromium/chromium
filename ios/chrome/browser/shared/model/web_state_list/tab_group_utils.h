// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_TAB_GROUP_UTILS_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_TAB_GROUP_UTILS_H_

#import <set>

class Browser;
class BrowserList;
class ProfileIOS;
class TabGroup;
class WebStateList;

namespace web {
class WebStateID;
}  // namespace web

// Returns all the TabGroups on all windows for the chosen mode (`incognito`).
std::set<const TabGroup*> GetAllGroupsForBrowserList(BrowserList* browser_list,
                                                     bool incognito);

// Returns all the TabGroups on all windows for the chosen mode (`incognito`).
std::set<const TabGroup*> GetAllGroupsForProfile(ProfileIOS* profile);

// Returns the tab group that contains the active web state in `web_state_list`.
// Returns `nullptr` if the active web state is not in a group.
const TabGroup* GetGroupForActiveWebState(WebStateList* web_state_list);

// Move the web state associated with `web_state_identifier` to
// `destination_group`, potentially moving the web state to a different window.
void MoveTabToGroup(web::WebStateID web_state_identifier,
                    const TabGroup* destination_group,
                    ProfileIOS* profile);

// Returns the Browser with `group` in its WebStateList. If `is_otr_group` is
// `true` then this will look for `group` inside Incognito browsers - otherwise
// it will look inside regular browsers. Returns `nullptr` if not found.
Browser* GetBrowserForGroup(BrowserList* browser_list,
                            const TabGroup* group,
                            bool is_otr_group);

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_TAB_GROUP_UTILS_H_
