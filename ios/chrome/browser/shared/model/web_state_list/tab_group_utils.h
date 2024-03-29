// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_TAB_GROUP_UTILS_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_TAB_GROUP_UTILS_H_

#import <set>

class ChromeBrowserState;
class TabGroup;

namespace web {
class WebStateID;
}
// Returns all the TabGroups on all windows for the chosen mode (`incognito`).
std::set<const TabGroup*> GetAllGroupsForBrowserState(
    ChromeBrowserState* browser_state);

// Move the web state associated with `web_state_identifier` to
// `destination_group`, potentially moving the web state to a different window.
void MoveTabToGroup(web::WebStateID web_state_identifier,
                    const TabGroup* destination_group,
                    ChromeBrowserState* browser_state);

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_TAB_GROUP_UTILS_H_
