// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TAB_GRID_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TAB_GRID_COMMANDS_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"
#import "ios/chrome/browser/shared/public/commands/tab_grid_commands.h"
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

// Shows the tab grid according to `page`.
- (void)showPage:(TabGridPage)page animated:(BOOL)animated;

// Prepares the TabGrid to exit.
- (void)prepareToExitTabGrid;

// Exits the tab grid, opening the selected tab of the current page (if
// relevant).
- (void)exitTabGrid;

// Displays the Guided Tour step that highlights the active tab. `completion`
// will be executed after the step dismisses.
- (void)showGuidedTourLongPressStepWithDismissalCompletion:
    (ProceduralBlock)completion;

// Hides the Guided Tour on the tab grid.
- (void)hideTabGridGuidedTour;

// Presents an IPH bubble to highlight pinning the active tab in the Tab Grid.
- (void)presentPinTabBubble;

// Presents an IPH bubble to highlight creating a tab group in the Tab Grid.
- (void)presentCreateTabGroupBubble;

// Displays the IPH instructing the user to swipe right to switch to incognito.
- (void)showSwipeToIncognitoIPH;

// Presents the page action menu from the tab grid, registering the source.
- (void)showPageActionMenuFromTabGrid;

// Activates the grid container's NSLayoutConstraints. To prevent a misalignment
// of the tab grid in iOS 27+, invoke this function after any tab grid
// animations are complete.
- (void)activateGridContainerConstraints;

// Deactivates the grid container's NSLayoutConstraints. To prevent a
// misalignment of the tab grid in iOS 27+, invoke this function during all tab
// grid animations.
- (void)deactivateGridContainerConstraints;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TAB_GRID_COMMANDS_H_
