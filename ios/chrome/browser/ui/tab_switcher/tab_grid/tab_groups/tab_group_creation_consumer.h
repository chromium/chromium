// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUP_CREATION_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUP_CREATION_CONSUMER_H_

#import "components/tab_groups/tab_group_color.h"

@class GroupTabInfo;

// Consumer to allow the tab group model to send information to the tab group
// UI when creating a tab group.
@protocol TabGroupCreationConsumer

// Sets the default group color. Should be called before viewDidLoad.
- (void)setDefaultGroupColor:(tab_groups::TabGroupColorId)color;
// Sets snapshots, favicons and the total number of selected items.
- (void)setTabGroupInfos:(NSArray<GroupTabInfo*>*)tabGroupInfos
    numberOfSelectedItems:(NSInteger)numberOfSelectedItems;
// Sets the default group color. Should be called before viewDidLoad.
- (void)setGroupTitle:(NSString*)title;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUP_CREATION_CONSUMER_H_
