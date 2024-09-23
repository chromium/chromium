// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_CELL_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_CELL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/commerce/ui_bundled/price_card/price_card_view.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_theme.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_context_menu/tab_cell.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/legacy_grid_to_tab_transition_view.h"

@class GridCell;

// Informs the receiver of actions on the cell.
@protocol GridCellDelegate
- (void)closeButtonTappedForCell:(GridCell*)cell;
@end

// A square-ish cell in a grid. Contains an icon, title, snapshot, and close
// button.
@interface GridCell : TabCell
// Delegate to inform the grid of actions on the cell.
@property(nonatomic, weak) id<GridCellDelegate> delegate;
// The look of the cell.
@property(nonatomic, assign) GridTheme theme;
// Settable UI elements of the cell.
@property(nonatomic, weak) UIImage* icon;
@property(nonatomic, weak) UIImage* snapshot;
@property(nonatomic, copy) NSString* title;
@property(nonatomic, assign) BOOL titleHidden;
// Sets to update and keep cell alpha in sync.
@property(nonatomic, assign) CGFloat opacity;
// The current state which the cell should display.
@property(nonatomic, assign) GridCellState state;
@property(nonatomic, weak) PriceCardView* priceCardView;

// Returns a transition selection cell with the same theme and frame as `cell`,
// but with no visible content view, no delegate, and no identifier.
//
// Note: Transition selection cell is a kind of "copy" of a GridCell to be used
// in the animated transitions that only shows selection state (that is, its
// content view is hidden).
+ (instancetype)transitionSelectionCellFromCell:(GridCell*)cell;

// Sets the price drop and displays the PriceViewCard.
- (void)setPriceDrop:(NSString*)price previousPrice:(NSString*)previousPrice;

// Fade in a new snapshot.
- (void)fadeInSnapshot:(UIImage*)snapshot;

// Starts the activity indicator animation.
- (void)showActivityIndicator;
// Stops the activity indicator animation.
- (void)hideActivityIndicator;
@end

@interface GridTransitionCell : GridCell <LegacyGridToTabTransitionView>
// Returns a cell with the same theme, icon, snapshot, title, and frame as
// `cell` (but no delegate or identifier) for use in animated transitions.
+ (instancetype)transitionCellFromCell:(GridCell*)cell;
@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_CELL_H_
