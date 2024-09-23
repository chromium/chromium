// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_REGULAR_REGULAR_GRID_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_REGULAR_REGULAR_GRID_MEDIATOR_H_

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_mediator.h"

// Mediates between model layer and regular grid UI layer.
@interface RegularGridMediator : BaseGridMediator

// Sends updates from the regular model layer to the inactive tabs model layer.
// This is needed, for example, when a user close all tabs from the regular grid
// as it also close all inactives tabs.
// TODO(crbug.com/40273478): Refactor these to be a mutator.
@property(nonatomic, weak) id<GridCommands> inactiveTabsGridCommands;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_REGULAR_REGULAR_GRID_MEDIATOR_H_
