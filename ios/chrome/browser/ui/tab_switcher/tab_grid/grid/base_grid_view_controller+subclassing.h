// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_BASE_GRID_VIEW_CONTROLLER_SUBCLASSING_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_BASE_GRID_VIEW_CONTROLLER_SUBCLASSING_H_

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_view_controller.h"

// TODO(crbug.com/1466000): Remove hard-coding of sections.
extern const int kGridOpenTabsSectionIndex;
extern NSString* const kGridOpenTabsSectionIdentifier;

// To ease the use of generics with the diffable data source, define a Snapshot
// type.
typedef NSDiffableDataSourceSnapshot<NSString*, GridItemIdentifier*> Snapshot;
typedef UICollectionViewDiffableDataSource<NSString*, GridItemIdentifier*>
    DiffableDataSource;

@interface BaseGridViewController (
    Subclassing) <UICollectionViewDelegate, UICollectionViewDelegateFlowLayout>

// A collection view of items in a grid format.
@property(nonatomic, weak, readonly) UICollectionView* collectionView;

// The collection view's data source.
@property(nonatomic, strong) DiffableDataSource* diffableDataSource;

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

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_BASE_GRID_VIEW_CONTROLLER_SUBCLASSING_H_
