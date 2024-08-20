// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/home_customization_collection_configurator.h"

#import "ios/chrome/browser/home_customization/ui/home_customization_view_controller_protocol.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_helper.h"

namespace {

// The height of a cell in a vertical collection view section.
const CGFloat kVerticalListCellHeight = 74;

// The horizontal spacing between the cell and each side of the vertical
// collection view.
const CGFloat kVerticalListHorizontalPadding = 20;

// The vertical spacing between cells.
const CGFloat kSpacingBetweenCells = 10;

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
  section.interGroupSpacing = kSpacingBetweenCells;
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
                             NSIndexPath* indexPath, NSNumber* itemIdentifier) {
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

@end
