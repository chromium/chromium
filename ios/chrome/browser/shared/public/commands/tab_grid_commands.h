// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TAB_GRID_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TAB_GRID_COMMANDS_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_paging.h"

class TabGroup;

// Command protocol related to the Tab Grid.
@protocol TabGridCommands

// Brings `group` into view by making it part of the visible element of its
// grid.
- (void)bringGroupIntoView:(const TabGroup*)group animated:(BOOL)animated;

// Shows the history searching for `text`.
- (void)showHistoryForText:(NSString*)text;

// Shows a non-incognito web page searching for `text`.
- (void)showWebSearchForText:(NSString*)text;

// Shows the recent tabs panel searching for `text`.
- (void)showRecentTabsForText:(NSString*)text;

// Shows the tab grid according to `page`.
- (void)showPage:(TabGridPage)page animated:(BOOL)animated;

// Exits the tab grid, opening the selected tab of the current page (if
// relevant).
- (void)exitTabGrid;

// Displays the Guided Tour step that highlights the active tab. `completion`
// will be executed after the step dismisses.
- (void)showGuidedTourLongPressStepWithDismissalCompletion:
    (ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TAB_GRID_COMMANDS_H_
