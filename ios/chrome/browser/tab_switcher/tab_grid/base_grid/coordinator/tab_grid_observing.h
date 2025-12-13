// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_TAB_GRID_BASE_GRID_COORDINATOR_TAB_GRID_OBSERVING_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_TAB_GRID_BASE_GRID_COORDINATOR_TAB_GRID_OBSERVING_H_

// Observer protocol for UI elements that want to respond to entering or exiting
// the tab grid.
@protocol TabGridObserving

@optional

// Called right before entering the tab grid.
- (void)willEnterTabGrid;

// Called right before exiting the tab grid.
- (void)willExitTabGrid;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_TAB_GRID_BASE_GRID_COORDINATOR_TAB_GRID_OBSERVING_H_
