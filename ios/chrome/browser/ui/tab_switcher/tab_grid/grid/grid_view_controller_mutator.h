// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_VIEW_CONTROLLER_MUTATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_VIEW_CONTROLLER_MUTATOR_H_

#import <UIKit/UIKit.h>

@class GridItemIdentifier;

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

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_VIEW_CONTROLLER_MUTATOR_H_
