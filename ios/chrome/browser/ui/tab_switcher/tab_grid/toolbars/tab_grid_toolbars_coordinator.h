// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TOOLBARS_TAB_GRID_TOOLBARS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TOOLBARS_TAB_GRID_TOOLBARS_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_commands_wrangler.h"

@protocol GridToolbarsMutator;
@class TabGridBottomToolbar;
@protocol TabGridToolbarsActionWrangler;
@protocol TabGridToolbarsDelegateWrangler;
@class TabGridTopToolbar;

// Coordinator to manage both TabGrid toolbars.
@interface TabGridToolbarsCoordinator
    : ChromeCoordinator <TabGridToolbarsCommandsWrangler>

// Search delegate.
@property(nonatomic, weak) id<UISearchBarDelegate> searchDelegate;

// The toolbars.
@property(nonatomic, strong) TabGridTopToolbar* topToolbar;
@property(nonatomic, strong) TabGridBottomToolbar* bottomToolbar;

// Mutator to handle toolbars modification.
@property(nonatomic, readonly, weak) id<GridToolbarsMutator> toolbarsMutator;

// Wrangler to manage actions/delegate, should be removed in a future
// refactoring. Those should be moved to the Grid once the grid has a direct
// connection to the toolbars.
// TODO(crbug.com/1456659): Remove those.
@property(nonatomic, weak) id<TabGridToolbarsActionWrangler> actionWrangler;
@property(nonatomic, weak) id<TabGridToolbarsDelegateWrangler> delegateWrangler;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TOOLBARS_TAB_GRID_TOOLBARS_COORDINATOR_H_
