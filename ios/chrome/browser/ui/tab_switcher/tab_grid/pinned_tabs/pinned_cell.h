// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_PINNED_TABS_PINNED_CELL_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_PINNED_TABS_PINNED_CELL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_context_menu/tab_cell.h"

// A cell for the pinned tabs view. Contains an icon, title, snapshot.
@interface PinnedCell : TabCell

// View for displaying the favicon.
@property(nonatomic, strong) UIImageView* faviconView;
// Title is displayed by this label.
@property(nonatomic, strong) UILabel* titleLabel;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_PINNED_TABS_PINNED_CELL_H_
