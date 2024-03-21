// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_GROUP_UTILS_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_GROUP_UTILS_H_

#import <UIKit/UIKit.h>

#import <set>

#import "components/tab_groups/tab_group_color.h"

class ChromeBrowserState;
class TabGroup;
class WebStateList;

namespace web {
class WebStateID;
}

// Returns all the colors a TabGroup can have.
std::vector<tab_groups::TabGroupColorId> AllPossibleTabGroupColors();

// Returns a color name based on a `tab_group_color_id`.
UIColor* ColorForTabGroupColorId(
    tab_groups::TabGroupColorId tab_group_color_id);

// Returns the default color for a new TabGroup in `web_state_list`. This is
// based on the colours currently used by this web state list (for this window).
tab_groups::TabGroupColorId DefaultColorForNewTabGroup(
    WebStateList* web_state_list);

// Returns all the TabGroups on all windows for the chosen mode (`incognito`).
std::set<const TabGroup*> GetAllGroupsForBrowserState(
    ChromeBrowserState* browser_state);

// Move the web state associated with `web_state_identifier` to
// `destination_group`, potentially moving the web state to a different windnow.
void MoveTabToGroup(web::WebStateID web_state_identifier,
                    const TabGroup* destination_group,
                    ChromeBrowserState* browser_state);

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_GROUP_UTILS_H_
