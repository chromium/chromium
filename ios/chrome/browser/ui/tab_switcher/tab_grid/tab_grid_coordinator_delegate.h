// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_COORDINATOR_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_paging.h"

class Browser;
@class TabGridCoordinator;

// This delegate is used to drive the TabSwitcher dismissal and execute code
// when the presentation and dismmiss animations finishes.
@protocol TabGridCoordinatorDelegate

// Informs the delegate the tab switcher that the given browser should be set to
// active.
- (void)tabGrid:(TabGridCoordinator*)tabGrid
    shouldActivateBrowser:(Browser*)browser
             focusOmnibox:(BOOL)focusOmnibox;

// Informs the delegate that the tab switcher is done and should be dismissed.
- (void)tabGridDismissTransitionDidEnd:(TabGridCoordinator*)tabGrid;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_COORDINATOR_DELEGATE_H_
