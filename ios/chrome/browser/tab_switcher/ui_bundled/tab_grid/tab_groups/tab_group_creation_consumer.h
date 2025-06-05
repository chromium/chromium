// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_TAB_GROUP_CREATION_CONSUMER_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_TAB_GROUP_CREATION_CONSUMER_H_

#import "components/tab_groups/tab_group_color.h"

@class TabSnapshotAndFavicon;

// Consumer to allow the tab group model to send information to the tab group
// UI when creating a tab group.
@protocol TabGroupCreationConsumer

// Sets the default group color. Should be called before viewDidLoad.
- (void)setDefaultGroupColor:(tab_groups::TabGroupColorId)color;

// Sets the default group color. Should be called before viewDidLoad.
- (void)setGroupTitle:(NSString*)title;

// Sets the `tabSnapshotAndFavicon` at `tabIndex`.
- (void)setSnapshotAndFavicon:(TabSnapshotAndFavicon*)tabSnapshotAndFavicon
                     tabIndex:(NSInteger)tabIndex;

// Sets the number of tabs in the group.
- (void)setTabsCount:(NSInteger)tabsCount;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_TAB_GROUP_CREATION_CONSUMER_H_
