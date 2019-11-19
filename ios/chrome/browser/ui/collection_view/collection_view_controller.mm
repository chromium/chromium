// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/collection_view/collection_view_controller.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/material_components/chrome_app_bar_view_controller.h"
#import "ios/chrome/browser/ui/material_components/utils.h"
#import "ios/chrome/common/colors/UIColor+cr_semantic_colors.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// The implementation of this controller follows the guidelines from
// https://github.com/material-components/material-components-ios/tree/develop/components/AppBar
@implementation CollectionViewController
@synthesize appBarViewController = _appBarViewController;
@synthesize collectionViewModel = _collectionViewModel;

- (instancetype)initWithLayout:(UICollectionViewLayout*)layout
                         style:(CollectionViewControllerStyle)style {
  self = [super initWithCollectionViewLayout:layout];
  if (self) {
    if (style == CollectionViewControllerStyleAppBar) {
      _appBarViewController = [[ChromeAppBarViewController alloc] init];
    }
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  // Configure the app bar, if there is one.
  if (self.appBarViewController) {
    // Configure the app bar style.
    ConfigureAppBarViewControllerWithCardStyle(self.appBarViewController);
    // Set the header view's tracking scroll view.
    self.appBarViewController.headerView.trackingScrollView =
        self.collectionView;
    // After all other views have been registered.
    [self addChildViewController:_appBarViewController];
    // Match the width of the parent view.
    CGRect frame = self.appBarViewController.view.frame;
    frame.origin.x = 0;
    frame.size.width =
        self.appBarViewController.parentViewController.view.bounds.size.width;
    self.appBarViewController.view.frame = frame;
    [self.view addSubview:self.appBarViewController.view];
    [self.appBarViewController didMoveToParentViewController:self];

    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(contentSizeCategoryDidChange:)
               name:UIContentSizeCategoryDidChangeNotification
             object:nil];
  }

  // Suport dark mode.
  self.collectionView.backgroundColor = UIColor.cr_systemGroupedBackgroundColor;
  self.styler.cellBackgroundColor =
      UIColor.cr_secondarySystemGroupedBackgroundColor;
}

- (void)contentSizeCategoryDidChange:(id)sender {
  [MDCCollectionViewCell cr_clearPreferredHeightForWidthCellCache];
  [self.collectionView.collectionViewLayout invalidateLayout];
}

- (UIViewController*)childViewControllerForStatusBarHidden {
  return self.appBarViewController;
}

- (UIViewController*)childViewControllerForStatusBarStyle {
  return self.appBarViewController;
}

- (void)loadModel {
  _collectionViewModel = [[CollectionViewModel alloc] init];
}

- (void)reconfigureCellsForItems:(NSArray*)items {
  for (CollectionViewItem* item in items) {
    NSIndexPath* indexPath = [self.collectionViewModel indexPathForItem:item];
    [self reconfigureCellAtIndexPath:indexPath withItem:item];
  }
}

- (void)reconfigureCellsAtIndexPaths:(NSArray*)indexPaths {
  for (NSIndexPath* indexPath in indexPaths) {
    CollectionViewItem* item =
        [self.collectionViewModel itemAtIndexPath:indexPath];
    [self reconfigureCellAtIndexPath:indexPath withItem:item];
  }
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 13, *)) {
    if ([self.traitCollection
            hasDifferentColorAppearanceComparedToTraitCollection:
                previousTraitCollection]) {
      // MDCCollectionView doesn't support dynamic colors, so they have to be
      // resolved now.
      // TODO(crbug.com/984928): Clean up once dynamic color support is added.
      self.styler.cellBackgroundColor =
          [UIColor.cr_secondarySystemGroupedBackgroundColor
              resolvedColorWithTraitCollection:self.traitCollection];
      [self.collectionViewLayout invalidateLayout];
    }
  }
}

#pragma mark MDCCollectionViewEditingDelegate

- (void)collectionView:(UICollectionView*)collectionView
    willDeleteItemsAtIndexPaths:(NSArray*)indexPaths {
  // Check that the parent class doesn't implement this method. Otherwise, it
  // would need to be called here.
  DCHECK([self isKindOfClass:[MDCCollectionViewController class]]);
  DCHECK(![MDCCollectionViewController instancesRespondToSelector:_cmd]);

  // Sort and enumerate in reverse order to delete the items from the collection
  // view model.
  NSArray* sortedIndexPaths =
      [indexPaths sortedArrayUsingSelector:@selector(compare:)];
  for (NSIndexPath* indexPath in [sortedIndexPaths reverseObjectEnumerator]) {
    NSInteger sectionIdentifier = [self.collectionViewModel
        sectionIdentifierForSection:indexPath.section];
    NSInteger itemType =
        [self.collectionViewModel itemTypeForIndexPath:indexPath];
    NSUInteger index =
        [self.collectionViewModel indexInItemTypeForIndexPath:indexPath];
    [self.collectionViewModel removeItemWithType:itemType
                       fromSectionWithIdentifier:sectionIdentifier
                                         atIndex:index];
  }
}

- (void)collectionView:(UICollectionView*)collectionView
    willMoveItemAtIndexPath:(NSIndexPath*)indexPath
                toIndexPath:(NSIndexPath*)newIndexPath {
  // Check that the parent class doesn't implement this method. Otherwise, it
  // would need to be called here.
  DCHECK([self isKindOfClass:[MDCCollectionViewController class]]);
  DCHECK(![MDCCollectionViewController instancesRespondToSelector:_cmd]);

  // Retain the item to be able to move it.
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];

  // Item coordinates.
  NSInteger sectionIdentifier =
      [self.collectionViewModel sectionIdentifierForSection:indexPath.section];
  NSInteger itemType =
      [self.collectionViewModel itemTypeForIndexPath:indexPath];
  NSUInteger indexInItemType =
      [self.collectionViewModel indexInItemTypeForIndexPath:indexPath];

  // Move the item.
  [self.collectionViewModel removeItemWithType:itemType
                     fromSectionWithIdentifier:sectionIdentifier
                                       atIndex:indexInItemType];
  NSInteger section = [self.collectionViewModel
      sectionIdentifierForSection:newIndexPath.section];
  [self.collectionViewModel insertItem:item
               inSectionWithIdentifier:section
                               atIndex:newIndexPath.item];
}

#pragma mark UICollectionViewDataSource

- (NSInteger)numberOfSectionsInCollectionView:
    (UICollectionView*)collectionView {
  return [self.collectionViewModel numberOfSections];
}

- (NSInteger)collectionView:(UICollectionView*)collectionView
     numberOfItemsInSection:(NSInteger)section {
  return [self.collectionViewModel numberOfItemsInSection:section];
}

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  Class cellClass = [item cellClass];
  NSString* reuseIdentifier = NSStringFromClass(cellClass);
  [self.collectionView registerClass:cellClass
          forCellWithReuseIdentifier:reuseIdentifier];
  MDCCollectionViewCell* cell = [self.collectionView
      dequeueReusableCellWithReuseIdentifier:reuseIdentifier
                                forIndexPath:indexPath];
  [item configureCell:cell];
  return cell;
}

- (UICollectionReusableView*)collectionView:(UICollectionView*)collectionView
          viewForSupplementaryElementOfKind:(NSString*)kind
                                atIndexPath:(NSIndexPath*)indexPath {
  CollectionViewItem* item = nil;
  UIAccessibilityTraits traits = UIAccessibilityTraitNone;
  if ([kind isEqualToString:UICollectionElementKindSectionHeader]) {
    item = [self.collectionViewModel headerForSection:indexPath.section];
    traits = UIAccessibilityTraitHeader;
  } else if ([kind isEqualToString:UICollectionElementKindSectionFooter]) {
    item = [self.collectionViewModel footerForSection:indexPath.section];
  } else {
    return [super collectionView:collectionView
        viewForSupplementaryElementOfKind:kind
                              atIndexPath:indexPath];
  }

  Class cellClass = [item cellClass];
  NSString* reuseIdentifier = NSStringFromClass(cellClass);
  [self.collectionView registerClass:cellClass
          forSupplementaryViewOfKind:kind
                 withReuseIdentifier:reuseIdentifier];
  MDCCollectionViewCell* cell = [self.collectionView
      dequeueReusableSupplementaryViewOfKind:kind
                         withReuseIdentifier:reuseIdentifier
                                forIndexPath:indexPath];
  [item configureCell:cell];
  cell.accessibilityTraits |= traits;
  return cell;
}

#pragma mark - UICollectionViewDelegateFlowLayout

- (CGSize)collectionView:(UICollectionView*)collectionView
                             layout:
                                 (UICollectionViewLayout*)collectionViewLayout
    referenceSizeForHeaderInSection:(NSInteger)section {
  CollectionViewItem* item =
      [self.collectionViewModel headerForSection:section];

  if (item) {
    // TODO(crbug.com/635604): Support arbitrary sized headers.
    return CGSizeMake(0, MDCCellDefaultOneLineHeight);
  }
  return CGSizeZero;
}

- (CGSize)collectionView:(UICollectionView*)collectionView
                             layout:
                                 (UICollectionViewLayout*)collectionViewLayout
    referenceSizeForFooterInSection:(NSInteger)section {
  CollectionViewItem* item =
      [self.collectionViewModel footerForSection:section];

  if (item) {
    // TODO(crbug.com/635604): Support arbitrary sized footers.
    return CGSizeMake(0, MDCCellDefaultOneLineHeight);
  }
  return CGSizeZero;
}

#pragma mark UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  MDCFlexibleHeaderView* headerView = self.appBarViewController.headerView;
  if (scrollView == headerView.trackingScrollView) {
    [headerView trackingScrollViewDidScroll];
  }
}

- (void)scrollViewDidEndDecelerating:(UIScrollView*)scrollView {
  MDCFlexibleHeaderView* headerView = self.appBarViewController.headerView;
  if (scrollView == headerView.trackingScrollView) {
    [headerView trackingScrollViewDidEndDecelerating];
  }
}

- (void)scrollViewDidEndDragging:(UIScrollView*)scrollView
                  willDecelerate:(BOOL)decelerate {
  MDCFlexibleHeaderView* headerView = self.appBarViewController.headerView;
  if (scrollView == headerView.trackingScrollView) {
    [headerView trackingScrollViewDidEndDraggingWillDecelerate:decelerate];
  }
}

- (void)scrollViewWillEndDragging:(UIScrollView*)scrollView
                     withVelocity:(CGPoint)velocity
              targetContentOffset:(inout CGPoint*)targetContentOffset {
  MDCFlexibleHeaderView* headerView = self.appBarViewController.headerView;
  if (scrollView == headerView.trackingScrollView) {
    [headerView
        trackingScrollViewWillEndDraggingWithVelocity:velocity
                                  targetContentOffset:targetContentOffset];
  }
}

#pragma mark - NSObject

- (NSString*)description {
  return self.collectionView.accessibilityIdentifier;
}

#pragma mark - Private

// Reconfigures the cell at |indexPath| by calling |configureCell:| with |item|.
- (void)reconfigureCellAtIndexPath:(NSIndexPath*)indexPath
                          withItem:(CollectionViewItem*)item {
  MDCCollectionViewCell* cell =
      base::mac::ObjCCastStrict<MDCCollectionViewCell>(
          [self.collectionView cellForItemAtIndexPath:indexPath]);

  // |cell| may be nil if the row is not currently on screen.
  if (cell) {
    [item configureCell:cell];
  }
}

@end
