// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TOOLBARS_TAB_GRID_TOOLBARS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TOOLBARS_TAB_GRID_TOOLBARS_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol GridToolbarsMutator;
@class TabGridBottomToolbar;
@class TabGridModeHolder;
@protocol TabGridToolbarsMainTabGridDelegate;
@class TabGridTopToolbar;
@protocol TabGridToolbarsCommandsWrangler;

// Coordinator to manage both TabGrid toolbars.
@interface TabGridToolbarsCoordinator : ChromeCoordinator

// Search delegate.
@property(nonatomic, weak) id<UISearchBarDelegate> searchDelegate;

// The toolbars.
@property(nonatomic, strong) TabGridTopToolbar* topToolbar;
@property(nonatomic, strong) TabGridBottomToolbar* bottomToolbar;

// Mutator to handle toolbars modification.
@property(nonatomic, readonly, weak) id<GridToolbarsMutator> toolbarsMutator;

// Action handler for the actions related to the tab grid .
@property(nonatomic, weak) id<TabGridToolbarsMainTabGridDelegate>
    toolbarTabGridDelegate;

// Holder for the TabGrid Mode.
@property(nonatomic, strong) TabGridModeHolder* modeHolder;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TOOLBARS_TAB_GRID_TOOLBARS_COORDINATOR_H_
