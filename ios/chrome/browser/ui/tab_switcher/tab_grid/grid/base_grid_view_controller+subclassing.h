// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_BASE_GRID_VIEW_CONTROLLER_SUBCLASSING_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_BASE_GRID_VIEW_CONTROLLER_SUBCLASSING_H_

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_layout.h"

// TODO(crbug.com/1466000): Remove hard-coding of sections.
extern const int kGridOpenTabsSectionIndex;
extern NSString* const kGridOpenTabsSectionIdentifier;

// To ease the use of generics with the diffable data source, define a Snapshot
// type.
typedef NSDiffableDataSourceSnapshot<NSString*, GridItemIdentifier*>
    GridSnapshot;
typedef UICollectionViewDiffableDataSource<NSString*, GridItemIdentifier*>
    GridDiffableDataSource;

@interface BaseGridViewController (Subclassing) <
    UICollectionViewDelegate,
    // TODO(crbug.com/1504112): Remove when the compositional layout is fully
    // landed.
    UICollectionViewDelegateFlowLayout>

// A collection view of items in a grid format.
@property(nonatomic, weak, readonly) UICollectionView* collectionView;

// The collection view's data source.
@property(nonatomic, strong) GridDiffableDataSource* diffableDataSource;

// Tracks if the items are in a batch action, which are the "Close All" or
// "Undo" the close all.
@property(nonatomic, readonly) BOOL isClosingAllOrUndoRunning;

// Creates the cell and supplementary view registrations and assigns them to the
// appropriate properties.
- (void)createRegistrations NS_REQUIRES_SUPER;

// Returns a configured header for the given index path.
- (UICollectionReusableView*)headerForSectionAtIndexPath:(NSIndexPath*)indexPath
    NS_REQUIRES_SUPER;

// Updates the ring to be around the currently selected item. If
// `shouldBringItemIntoView` is true, the collection view scrolls to present the
// selected item at the top.
- (void)updateSelectedCollectionViewItemRingAndBringIntoView:
    (BOOL)shouldBringItemIntoView;

// Returns the type of header to set in the given mode, in the current state of
// the grid.
// TODO(crbug.com/1504153): Refactor to avoid reusing the same section
// definition for the different use cases.
- (TabsSectionHeaderType)tabsSectionHeaderTypeForMode:(TabGridMode)mode;

// Updates the layout with the tabs section header type returned by
// `-tabsSectionHeaderTypeForMode:` with the current `mode`.
// TODO(crbug.com/1504153): Refactor to avoid reusing the same section
// definition for the different use cases.
- (void)updateTabsSectionHeaderType;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_BASE_GRID_VIEW_CONTROLLER_SUBCLASSING_H_
