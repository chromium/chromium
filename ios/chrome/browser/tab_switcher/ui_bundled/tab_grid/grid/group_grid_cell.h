// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_GRID_GROUP_GRID_CELL_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_GRID_GROUP_GRID_CELL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_theme.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_context_menu/tab_cell.h"

@protocol FacePileProviding;
@class GroupGridCell;
typedef NS_ENUM(NSInteger, EmptyThumbnailLayoutType);
@class TabSnapshotAndFavicon;

// Informs the receiver of actions on the cell.
@protocol GroupGridCellDelegate
- (void)closeButtonTappedForGroupCell:(GroupGridCell*)cell;
@end

// A square-ish cell in a grid. Contains the group's favicon, its title and
// close button.
@interface GroupGridCell : TabCell
// Delegate to inform the grid of actions on the cell.
@property(nonatomic, weak) id<GroupGridCellDelegate> delegate;
// The look of the cell.
@property(nonatomic, assign) GridTheme theme;
// Settable UI elements of the group cell.
@property(nonatomic, copy) UIColor* groupColor;
@property(nonatomic, copy) NSString* title;
@property(nonatomic, assign) NSInteger tabsCount;
// Sets to update and keep cell alpha in sync.
@property(nonatomic, assign) CGFloat opacity;
// The current state which the cell should display.
@property(nonatomic, assign) GridCellState state;
// The current layout configuration that should be used by the empty thumbnail.
@property(nonatomic, assign) EmptyThumbnailLayoutType layoutType;
// The FacePileProvider, to be set externally. Held as a strong reference to
// ensure the provider's lifecycle is maintained for managing and updating the
// FacePileView's content.
@property(nonatomic, strong) id<FacePileProviding> facePileProvider;

// Assigns a `TabSnapshotAndFavicon` object to a specific `tabIndex`
- (void)configureTabSnapshotAndFavicon:
            (TabSnapshotAndFavicon*)tabSnapshotAndFavicon
                              tabIndex:(NSInteger)tabIndex;

// Returns all tab views that compose this tab group view in the order they're
// presented.
- (NSArray<UIView*>*)allGroupTabViews;

// Highlights or resets the highlighting of the cell.
- (void)setHighlightForGrouping:(BOOL)highlight;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_GRID_GROUP_GRID_CELL_H_
