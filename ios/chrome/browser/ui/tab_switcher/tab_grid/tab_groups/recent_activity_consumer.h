// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_RECENT_ACTIVITY_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_RECENT_ACTIVITY_CONSUMER_H_

@class RecentActivityLogItem;

// Consumer to allow the recent activity model to send the information to its
// UI.
@protocol RecentActivityConsumer

// Populate all items of recent activity logs.
- (void)populateItems:(NSArray<RecentActivityLogItem*>*)items;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_RECENT_ACTIVITY_CONSUMER_H_
