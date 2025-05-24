// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_TAB_DELEGATE_H_
#define IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_TAB_DELEGATE_H_

// The Tabs delegate.
@protocol SideSwipeTabDelegate

// Notifies the mutator that a tab switch is about to occur due to a swipe
// gesture
- (void)willTabSwitchWithSwipeToTabIndex:(int)newTabIndex;

// Notifies the delegate that a tab switch should be performed to the given tab
// index due to a swipe.
- (void)tabSwitchWithSwipeToTabIndex:(int)newTabIndex;

// Notifies the delegate that a tab switcg with a swipe is cancelled and the
// navigation should revert to the initial tab index.
- (void)cancelTabSwitchWithSwipeAndRevertToInitialTabIndex:(int)initialTabIndex;

// Notifies the delegate that a tab switch with a swipe is completed.
- (void)didCompleteTabSwitchWithSwipe;

// Updates the current active tab snapshot for the tab switcher.
- (void)updateActiveTabSnapshot;

// Returns the current active tab index.
- (int)activeTabIndex;

// Returns how many tabs are there.
- (int)tabCount;

@end

#endif  // IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_TAB_DELEGATE_H_
