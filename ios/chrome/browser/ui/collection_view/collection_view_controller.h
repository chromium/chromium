// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COLLECTION_VIEW_COLLECTION_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_COLLECTION_VIEW_COLLECTION_VIEW_CONTROLLER_H_

#import <MaterialComponents/MaterialCollections.h>

#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"

@class CollectionViewItem;

typedef NS_ENUM(NSInteger, CollectionViewControllerStyle) {
  // A simple collection view controller.
  CollectionViewControllerStyleDefault,
};

// Chrome-specific Material Design collection view controller. With
// CollectionViewControllerStyleAppBar, it features an app bar in the card
// style.
@interface CollectionViewController : MDCCollectionViewController

// The model of this controller.
@property(strong, nonatomic, readonly)
    CollectionViewModel<CollectionViewItem*>* collectionViewModel;

// Initializer with the desired style and layout.
- (instancetype)initWithLayout:(UICollectionViewLayout*)layout
                         style:(CollectionViewControllerStyle)style
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithCollectionViewLayout:(UICollectionViewLayout*)layout
    NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

// Initializes the collection view model. Must be called by subclasses if they
// override this method in order to get a clean collectionViewModel.
- (void)loadModel NS_REQUIRES_SUPER;

// Reconfigures the cells corresponding to the given `items` by calling
// `configureCell:` on each cell.
- (void)reconfigureCellsForItems:(NSArray*)items;

// Reconfigures the cells corresponding to the given `indexPaths` by calling
// `configureCell:` on each cell.
- (void)reconfigureCellsAtIndexPaths:(NSArray*)indexPaths;

#pragma mark MDCCollectionViewEditingDelegate

// Updates the model with changes to the collection view items. Must be called
// by subclasses if they override this method in order to maintain this
// functionality.
- (void)collectionView:(UICollectionView*)collectionView
    willDeleteItemsAtIndexPaths:(NSArray*)indexPaths NS_REQUIRES_SUPER;

// Updates the model with changes to the collection view item. Must be called
// by subclasses if they override this method in order to maintain this
// functionality.
- (void)collectionView:(UICollectionView*)collectionView
    willMoveItemAtIndexPath:(NSIndexPath*)indexPath
                toIndexPath:(NSIndexPath*)newIndexPath NS_REQUIRES_SUPER;

@end

#endif  // IOS_CHROME_BROWSER_UI_COLLECTION_VIEW_COLLECTION_VIEW_CONTROLLER_H_
