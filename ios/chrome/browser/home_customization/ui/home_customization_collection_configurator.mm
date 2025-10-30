// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/home_customization_collection_configurator.h"

#import <cmath>

#import "ios/chrome/browser/home_customization/ui/home_customization_view_controller_protocol.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_helper.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"

namespace {

// The height of a cell in a vertical collection view section.
const CGFloat kVerticalListCellHeight = 74;

// Height for a background cell when the app is in portrait orientation.
const CGFloat kPortraitBackgroundCellHeight = 201;

// Minimum width for a background cell when the app is in portrait rotation.
// Depending on the aspect ratio of the window, the actual width may be larger.
const CGFloat kPortraitBackgroundCellMinimumWidth = 100;

// Width for a background cell when the app is in landscape orientation.
const CGFloat kLandscapeBackgroundCellWidth = 190;

// Minimum height for a background cell when the app is in landscape rotation.
// Depending on the aspect ratio of the window, the actual height may be larger.
const CGFloat kLandscapeBackgroundCellMinimumHeight = 108;

// The horizontal spacing between the cell and each side of the vertical
// collection view.
const CGFloat kVerticalListHorizontalPadding = 20;

// The horizontal spacing between the cell and each side of the horizontal
// collection view.
const CGFloat kHorizontalListHorizontalPadding = 20;

// The vertical spacing between the cell and the top/bottom of the horizontal
// collection view.
const CGFloat kHorizontalListVerticalPadding = 28;

// The vertical spacing between cells.
const CGFloat kVerticalSpacingBetweenCells = 10;

// The horizontal spacing between cells.
const CGFloat kHorizontalSpacingBetweenCells = 0;

// The vertical spacing below the header.
const CGFloat kSpacingBelowHeader = 10;

}  // namespace

@interface HomeCustomizationCollectionConfigurator ()

// The view controller being configured.
@property(nonatomic, weak)
    UIViewController<HomeCustomizationViewControllerProtocol,
                     UICollectionViewDelegate>* viewController;

@end

@implementation HomeCustomizationCollectionConfigurator

- (instancetype)initWithViewController:
    (UIViewController<HomeCustomizationViewControllerProtocol,
                      UICollectionViewDelegate>*)viewController {
  self = [super init];
  if (self) {
    _viewController = viewController;
  }
  return self;
}

#pragma mark - Public

- (void)configureCollectionView {
  UICollectionView* collectionView =
      [[UICollectionView alloc] initWithFrame:CGRectZero
                         collectionViewLayout:[self collectionViewLayout]];
  collectionView.translatesAutoresizingMaskIntoConstraints = NO;
  collectionView.delegate = _viewController;
  collectionView.accessibilityIdentifier = [HomeCustomizationHelper
      accessibilityIdentifierForPageCollection:_viewController.page];

  __weak __typeof(self) weakSelf = self;
  _viewController.additionalViewWillTransitionToSizeHandler =
      ^(CGSize size, id<UIViewControllerTransitionCoordinator> coordinator) {
        [weakSelf collectionViewWillTransitionToSize:size
                           withTransitionCoordinator:coordinator];
      };

  _viewController.collectionView = collectionView;

  UICollectionViewDiffableDataSource* diffableDataSource =
      [self createDiffableDataSource];
  _viewController.collectionView.dataSource = diffableDataSource;
  _viewController.diffableDataSource = diffableDataSource;
}

- (void)configureNavigationBar {
  _viewController.title =
      [HomeCustomizationHelper navigationBarTitleForPage:_viewController.page];
  UIBarButtonItem* dismissButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemClose
                           target:_viewController
                           action:@selector(dismissCustomizationMenuPage)];
  dismissButton.accessibilityIdentifier = kNavigationBarDismissButtonIdentifier;
  _viewController.navigationItem.rightBarButtonItem = dismissButton;
  _viewController.navigationItem.backBarButtonItem.accessibilityIdentifier =
      kNavigationBarBackButtonIdentifier;
  [_viewController.navigationItem setHidesBackButton:YES];
}

- (NSCollectionLayoutSection*)
    backgroundCellSectionForLayoutEnvironment:
        (id<NSCollectionLayoutEnvironment>)layoutEnvironment
                                   windowSize:(CGSize)windowSize {
  CGSize cellSize;
  if (windowSize.height > windowSize.width) {
    CGFloat cellHeight = kPortraitBackgroundCellHeight;
    CGFloat cellWidth = cellHeight / windowSize.height * windowSize.width;
    cellSize = CGSizeMake(
        std::fmax(cellWidth, kPortraitBackgroundCellMinimumWidth), cellHeight);
  } else {
    CGFloat cellWidth = kLandscapeBackgroundCellWidth;
    CGFloat cellHeight = cellWidth / windowSize.width * windowSize.height;
    cellSize = CGSizeMake(
        cellWidth,
        std::fmax(cellHeight, kLandscapeBackgroundCellMinimumHeight));
  }

  NSCollectionLayoutSize* itemSize = [NSCollectionLayoutSize
      sizeWithWidthDimension:[NSCollectionLayoutDimension
                                 absoluteDimension:cellSize.width]
             heightDimension:[NSCollectionLayoutDimension
                                 absoluteDimension:cellSize.height]];

  NSCollectionLayoutItem* item =
      [NSCollectionLayoutItem itemWithLayoutSize:itemSize];

  NSCollectionLayoutGroup* group =
      [NSCollectionLayoutGroup horizontalGroupWithLayoutSize:item.layoutSize
                                                    subitems:@[ item ]];

  // Create section and enable horizontal scrolling.
  NSCollectionLayoutSection* section =
      [NSCollectionLayoutSection sectionWithGroup:group];

  section.orthogonalScrollingBehavior =
      UICollectionLayoutSectionOrthogonalScrollingBehaviorContinuous;

  section.interGroupSpacing = kHorizontalSpacingBetweenCells;

  section.contentInsets = NSDirectionalEdgeInsetsMake(
      -kSpacingBelowHeader, kHorizontalListHorizontalPadding,
      kHorizontalListVerticalPadding, kHorizontalListHorizontalPadding);

  return section;
}

- (NSCollectionLayoutSection*)verticalListSectionForLayoutEnvironment:
    (id<NSCollectionLayoutEnvironment>)layoutEnvironment {
  NSCollectionLayoutSize* itemSize = [NSCollectionLayoutSize
      sizeWithWidthDimension:[NSCollectionLayoutDimension
                                 fractionalWidthDimension:1.]
             heightDimension:[NSCollectionLayoutDimension
                                 fractionalHeightDimension:1.]];
  NSCollectionLayoutItem* item =
      [NSCollectionLayoutItem itemWithLayoutSize:itemSize];

  NSCollectionLayoutSize* groupSize = [NSCollectionLayoutSize
      sizeWithWidthDimension:[NSCollectionLayoutDimension
                                 fractionalWidthDimension:1.]
             heightDimension:[NSCollectionLayoutDimension
                                 estimatedDimension:kVerticalListCellHeight]];
  NSCollectionLayoutGroup* group =
      [NSCollectionLayoutGroup verticalGroupWithLayoutSize:groupSize
                                                  subitems:@[ item ]];

  NSCollectionLayoutSection* section =
      [NSCollectionLayoutSection sectionWithGroup:group];

  // Adds spacing between cells, as well as content insets so that the cells
  // have the correct width.
  section.interGroupSpacing = kVerticalSpacingBetweenCells;
  section.contentInsets = NSDirectionalEdgeInsetsMake(
      [self doesPageHaveHeader] ? kSpacingBelowHeader : 0,
      kVerticalListHorizontalPadding, 0, kVerticalListHorizontalPadding);

  if ([self doesPageHaveHeader]) {
    NSCollectionLayoutSize* headerSize = [NSCollectionLayoutSize
        sizeWithWidthDimension:[NSCollectionLayoutDimension
                                   fractionalWidthDimension:1.]
               heightDimension:[NSCollectionLayoutDimension
                                   estimatedDimension:kVerticalListCellHeight]];
    NSCollectionLayoutBoundarySupplementaryItem* headerItem =
        [NSCollectionLayoutBoundarySupplementaryItem
            boundarySupplementaryItemWithLayoutSize:headerSize
                                        elementKind:
                                            UICollectionElementKindSectionHeader
                                          alignment:NSRectAlignmentTopLeading];
    section.boundarySupplementaryItems = @[ headerItem ];
  }

  return section;
}

#pragma mark - Private

// Returns a collection view layout suited for the current page.
- (UICollectionViewLayout*)collectionViewLayout {
  UICollectionViewCompositionalLayoutConfiguration* configuration =
      [[UICollectionViewCompositionalLayoutConfiguration alloc] init];
  __weak auto weakViewController = _viewController;
  return [[UICollectionViewCompositionalLayout alloc]
      initWithSectionProvider:^(
          NSInteger sectionIndex,
          id<NSCollectionLayoutEnvironment> layoutEnvironment) {
        return [weakViewController sectionForIndex:sectionIndex
                                 layoutEnvironment:layoutEnvironment];
      }
                configuration:configuration];
}

// Creates the diffable data source with a cell provider used to configure
// each cell.
- (UICollectionViewDiffableDataSource*)createDiffableDataSource {
  __weak auto weakViewController = _viewController;
  auto cellProvider =
      ^UICollectionViewCell*(UICollectionView* collectionView,
                             NSIndexPath* indexPath, NSString* itemIdentifier) {
        return [weakViewController configuredCellForIndexPath:indexPath
                                               itemIdentifier:itemIdentifier];
      };
  UICollectionViewDiffableDataSource* diffableDataSource =
      [[UICollectionViewDiffableDataSource alloc]
          initWithCollectionView:_viewController.collectionView
                    cellProvider:cellProvider];
  if ([self doesPageHaveHeader]) {
    diffableDataSource.supplementaryViewProvider = ^UICollectionReusableView*(
        UICollectionView* collectionView, NSString* elementKind,
        NSIndexPath* indexPath) {
      return [weakViewController configuredHeaderForIndexPath:indexPath];
    };
  }

  return diffableDataSource;
}

// Whether the current page should display a header.
- (BOOL)doesPageHaveHeader {
  return _viewController.page == CustomizationMenuPage::kDiscover ||
         _viewController.page == CustomizationMenuPage::kMagicStack;
}

- (void)collectionViewWillTransitionToSize:(CGSize)size
                 withTransitionCoordinator:
                     (id<UIViewControllerTransitionCoordinator>)coordinator {
  // Some of the layout sections care about rotation/size changes, so invalidate
  // the layout so those sections can be updated.
  [_viewController.collectionView.collectionViewLayout invalidateLayout];
}

@end
