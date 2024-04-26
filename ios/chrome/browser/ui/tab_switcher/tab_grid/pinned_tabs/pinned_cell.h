// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_PINNED_TABS_PINNED_CELL_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_PINNED_TABS_PINNED_CELL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_context_menu/tab_cell.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/legacy_grid_to_tab_transition_view.h"

namespace web {
class WebStateID;
}  // namespace web

// A cell for the pinned tabs view. Contains an icon, title, snapshot.
@interface PinnedCell : TabCell

// Returns a transition selection cell with the same frame as `cell`, but with
// no visible content view, no delegate, and no identifier.
//
// Note: Transition selection cell is a kind of "copy" of a PinnedCell to be
// used in the animated transitions that only shows selection state (that is,
// its content view is hidden).
+ (instancetype)transitionSelectionCellFromCell:(PinnedCell*)cell;

// Settable UI elements of the cell.
@property(nonatomic, strong) UIImage* icon;
@property(nonatomic, strong) UIImage* snapshot;
@property(nonatomic, copy) NSString* title;
@property(nonatomic, assign) web::WebStateID pinnedItemIdentifier;

// Starts the activity indicator animation.
- (void)showActivityIndicator;
// Stops the activity indicator animation.
- (void)hideActivityIndicator;

@end

// A "copy" of a PinnedCell to be used in the animated transitions. Some
// of the properties of PinnedTransitionCell are tweaked (compared to the
// PinnedCell) in order to provide a smooth transition animation.
//
// Note: This class is put into the same header/implementation file with
// the PinnedCell class in order to maintain the consistency with
// GridTransitionCell. Also this way PinnedTransitionCell has easier access to
// some of the internal properties of the PinnedCell. If PinnedTransitionCell
// is moved into its own file the same should be done with GridTransitionCell.
//
// TODO(crbug.com/40890700): Refactor `Transition` cells into separate header
// and implementation files.
@interface PinnedTransitionCell : PinnedCell <LegacyGridToTabTransitionView>

// Returns a cell with the same theme, icon, snapshot, title, and frame as
// `cell` (but no delegate or identifier) for use in animated transitions.
+ (instancetype)transitionCellFromCell:(PinnedCell*)cell;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_PINNED_TABS_PINNED_CELL_H_
