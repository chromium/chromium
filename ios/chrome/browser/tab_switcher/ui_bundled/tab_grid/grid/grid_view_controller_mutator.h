// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_GRID_GRID_VIEW_CONTROLLER_MUTATOR_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_GRID_GRID_VIEW_CONTROLLER_MUTATOR_H_

#import <UIKit/UIKit.h>

@class GridItemIdentifier;
class TabGroup;
@class TabGroupInfo;
@class TabInfo;

// Reflects user’s change in grid's model.
@protocol GridViewControllerMutator <NSObject>

// Notifies the model when the user tapped on a specific item id.
- (void)userTappedOnItemID:(GridItemIdentifier*)itemID;

// Adds the given `itemID` to the selected item lists.
- (void)addToSelectionItemID:(GridItemIdentifier*)itemID;

// Removes the given `itemID` to the selected item lists.
- (void)removeFromSelectionItemID:(GridItemIdentifier*)itemID;

// Notifies the model to close a specific item identifier.
- (void)closeItemWithIdentifier:(GridItemIdentifier*)identifier;

// Creates a group with `title` for `sourceItem` (or `droppedTab` if
// `sourceItem` is from another window) as the dropped tab and `destinationItem`
// as the tab that is visually replaced with the group.
- (void)createTabGroupWithTitle:(NSString*)title
                     sourceItem:(GridItemIdentifier*)sourceItem
                     droppedTab:(TabInfo*)droppedTab
                destinationItem:(GridItemIdentifier*)destinationItem;

// Adds `droppedTab` (of `sourceItem` if it is a local drop) to `group`.
- (void)addDroppedTab:(TabInfo*)droppedTab
           sourceItem:(GridItemIdentifier*)sourceItem
              toGroup:(const TabGroup*)group;

// Merge `droppedGroup` into `destinationItem`.
- (void)mergeGroup:(TabGroupInfo*)droppedGroup
    intoDestinationItem:(GridItemIdentifier*)destinationItem;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_GRID_GRID_VIEW_CONTROLLER_MUTATOR_H_
