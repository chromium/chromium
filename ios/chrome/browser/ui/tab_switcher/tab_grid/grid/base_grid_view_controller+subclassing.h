// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_BASE_GRID_VIEW_CONTROLLER_SUBCLASSING_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_BASE_GRID_VIEW_CONTROLLER_SUBCLASSING_H_

#import "ios/chrome/browser/ui/menu/menu_histograms.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_cell.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_layout.h"

// To ease the use of generics with the diffable data source, define a Snapshot
// type.
typedef NSDiffableDataSourceSnapshot<NSString*, GridItemIdentifier*>
    GridSnapshot;
typedef UICollectionViewDiffableDataSource<NSString*, GridItemIdentifier*>
    GridDiffableDataSource;

@interface BaseGridViewController (Subclassing) <GridCellDelegate,
                                                 UICollectionViewDragDelegate,
                                                 UICollectionViewDelegate>

// A collection view of items in a grid format.
@property(nonatomic, weak, readonly) UICollectionView* collectionView;

// The collection view's data source.
@property(nonatomic, strong) GridDiffableDataSource* diffableDataSource;

// Tracks if the items are in a batch action, which are the "Close All" or
// "Undo" the close all.
@property(nonatomic, readonly) BOOL isClosingAllOrUndoRunning;

// The current mode for the grid.
@property(nonatomic, assign, readonly) TabGridMode mode;

// Creates the cell and supplementary view registrations and assigns them to the
// appropriate properties.
- (void)createRegistrations NS_REQUIRES_SUPER;

// Returns a configured header for the given index path.
- (UICollectionReusableView*)headerForSectionAtIndexPath:
    (NSIndexPath*)indexPath;

// Returns a configured cell for the given `indexPath` and `itemIdentifier`. The
// subclass must call super if it can't handle it.
- (UICollectionViewCell*)cellForItemAtIndexPath:(NSIndexPath*)indexPath
                                 itemIdentifier:
                                     (GridItemIdentifier*)itemIdentifier
    NS_REQUIRES_SUPER;

// Updates the ring to be around the currently selected item. If
// `shouldBringItemIntoView` is true, the collection view scrolls to present the
// selected item at the top.
- (void)updateSelectedCollectionViewItemRingAndBringIntoView:
    (BOOL)shouldBringItemIntoView;

// Returns the type of header to set in the given mode, in the current state of
// the grid.
// TODO(crbug.com/40944664): Refactor to avoid reusing the same section
// definition for the different use cases.
- (TabsSectionHeaderType)tabsSectionHeaderTypeForMode:(TabGridMode)mode;

// Updates the layout with the tabs section header type returned by
// `-tabsSectionHeaderTypeForMode:` with the current `mode`.
// TODO(crbug.com/40944664): Refactor to avoid reusing the same section
// definition for the different use cases.
- (void)updateTabsSectionHeaderType;

// Returns the number of tabs in the collection view.
- (NSInteger)numberOfTabs;

// Provides an opportunity to the subclasses to add items to `snapshot`.
- (void)addAdditionalItemsToSnapshot:(GridSnapshot*)snapshot;

// Provides an opportunity to update `snapshot` after an update of the grid's
// mode.
- (void)updateSnapshotForModeUpdate:(GridSnapshot*)snapshot;

// Returns the scenario histogram to be used to display a context menu.
- (MenuScenarioHistogram)scenarioForContextMenu;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_BASE_GRID_VIEW_CONTROLLER_SUBCLASSING_H_
