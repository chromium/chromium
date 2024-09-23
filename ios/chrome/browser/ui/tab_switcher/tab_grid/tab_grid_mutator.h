// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_MUTATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_MUTATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_metrics.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_paging.h"

// Allows the tab grid view controller to reflect user’s change in the model.
@protocol TabGridMutator <NSObject>

// Notify the model that the user changed the displayed page (incognito, regular
// or recent).
- (void)pageChanged:(TabGridPage)currentPage
        interaction:(TabSwitcherPageChangeInteraction)interaction;

// Notify the model that a drag and drop session started or ended.
- (void)dragAndDropSessionStarted;
- (void)dragAndDropSessionEnded;

// Leaves the search mode.
- (void)quitSearchMode;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_MUTATOR_H_
