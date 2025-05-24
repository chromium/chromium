// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_UI_TAB_GROUP_UTILS_H_
#define IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_UI_TAB_GROUP_UTILS_H_

#import <UIKit/UIKit.h>

#import <vector>

namespace tab_groups {

enum class TabGroupColorId;

// Returns all the colors a TabGroup can have.
std::vector<TabGroupColorId> AllPossibleTabGroupColors();

// Returns a UIColor based on a `tab_group_color_id`.
UIColor* ColorForTabGroupColorId(TabGroupColorId tab_group_color_id);

// Returns a UIColor for the text to be displayed on top a
// `tab_group_color_id` color.
UIColor* ForegroundColorForTabGroupColorId(TabGroupColorId tab_group_color_id);

}  // namespace tab_groups

#endif  // IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_UI_TAB_GROUP_UTILS_H_
