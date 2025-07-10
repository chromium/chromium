// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_TAB_DELEGATE_H_
#define IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_TAB_DELEGATE_H_

#import "base/ios/block_types.h"

// The Tabs delegate.
@protocol SideSwipeTabDelegate

// Notifies the mutator that a tab switch is about to occur due to a swipe
// gesture
- (void)willTabSwitchWithSwipeToTabIndex:(int)newTabIndex;

// Notifies the delegate that a tab switch should be performed to the given tab
// index due to a swipe.
- (void)tabSwitchWithSwipeToTabIndex:(int)newTabIndex;

// Notifies the delegate that a tab switch with a swipe is completed.
- (void)didCompleteTabSwitchWithSwipe;

// Updates the current active tab snapshot for the tab switcher with `callback`
// to run after the snapshotting is complete.
- (void)updateActiveTabSnapshot:(ProceduralBlock)callback;

// Returns the current active tab index.
- (int)activeTabIndex;

// Returns how many tabs are there.
- (int)tabCount;

@end

#endif  // IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_TAB_DELEGATE_H_
