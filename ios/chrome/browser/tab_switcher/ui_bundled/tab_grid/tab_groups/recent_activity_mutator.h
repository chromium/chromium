// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_RECENT_ACTIVITY_MUTATOR_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_RECENT_ACTIVITY_MUTATOR_H_

@class RecentActivityLogItem;

// Mutator for the Recent Activity.
@protocol RecentActivityMutator

// Performs the action associated with `item`.
- (void)performActionForItem:(RecentActivityLogItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_RECENT_ACTIVITY_MUTATOR_H_
