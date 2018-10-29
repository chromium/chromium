// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_RECENT_TABS_RECENT_TABS_TABLE_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_RECENT_TABS_RECENT_TABS_TABLE_VIEW_CONTROLLER_DELEGATE_H_

#import <Foundation/Foundation.h>

// Protocol used by RecentTabsTableViewController to communicate to its
// mediator.
@protocol RecentTabsTableViewControllerDelegate<NSObject>

// Tells the delegate to refresh the session view.
- (void)refreshSessionsView;

@end

#endif  // IOS_CHROME_BROWSER_UI_RECENT_TABS_RECENT_TABS_TABLE_VIEW_CONTROLLER_DELEGATE_H_
