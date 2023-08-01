// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_RECENT_TABS_RECENT_TABS_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_RECENT_TABS_RECENT_TABS_COORDINATOR_DELEGATE_H_

@class RecentTabsCoordinator;

// Delegate for the recent tabs coordinator
@protocol RecentTabsCoordinatorDelegate <NSObject>

// Requests the delegate to dismiss the coordinator.
- (void)recentTabsCoordinatorWantsToBeDismissed:
    (RecentTabsCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_UI_RECENT_TABS_RECENT_TABS_COORDINATOR_DELEGATE_H_
