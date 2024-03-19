// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_GROUP_UTILS_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_GROUP_UTILS_H_

#import <UIKit/UIKit.h>

#import "components/tab_groups/tab_group_color.h"

class WebStateList;

// Returns all the colors a TabGroup can have.
std::vector<tab_groups::TabGroupColorId> AllPossibleTabGroupColors();

// Returns a color name based on a `tab_group_color_id`.
UIColor* ColorForTabGroupColorId(
    tab_groups::TabGroupColorId tab_group_color_id);

// Returns the default color for a new TabGroup in `web_state_list`.
tab_groups::TabGroupColorId DefaultColorForNewTabGroup(
    WebStateList* web_state_list);

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_GROUP_UTILS_H_
