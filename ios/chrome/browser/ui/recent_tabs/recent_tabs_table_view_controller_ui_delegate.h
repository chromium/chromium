// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_RECENT_TABS_RECENT_TABS_TABLE_VIEW_CONTROLLER_UI_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_RECENT_TABS_RECENT_TABS_TABLE_VIEW_CONTROLLER_UI_DELEGATE_H_

@class RecentTabsTableViewController;

// Delegate for the RecentTabsTableViewController for all UI-related events.
@protocol RecentTabsTableViewControllerUIDelegate

// Tells the delegate that the scroll view scrolled.
- (void)recentTabsScrollViewDidScroll:
    (RecentTabsTableViewController*)recentTabsTableViewController;

@end

#endif  // IOS_CHROME_BROWSER_UI_RECENT_TABS_RECENT_TABS_TABLE_VIEW_CONTROLLER_UI_DELEGATE_H_
