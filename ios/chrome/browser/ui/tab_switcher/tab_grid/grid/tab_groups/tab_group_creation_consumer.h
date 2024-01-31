// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_TAB_GROUPS_TAB_GROUP_CREATION_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_TAB_GROUPS_TAB_GROUP_CREATION_CONSUMER_H_

#import "components/tab_groups/tab_group_color.h"

// Consumer to allow the tab group model to send information to the tab group
// UI when creating a tab group.
@protocol TabGroupCreationConsumer

// Sets the default group color.
- (void)setDefaultGroupColor:(tab_groups::TabGroupColorId)color;
// Sets snapshots and favicons.
- (void)setSnapshots:(NSArray*)snapshots favicons:(NSArray*)favicons;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_TAB_GROUPS_TAB_GROUP_CREATION_CONSUMER_H_
