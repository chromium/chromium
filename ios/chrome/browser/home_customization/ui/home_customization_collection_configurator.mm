// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/home_customization_collection_configurator.h"

namespace {

// The dimensions of a cell in a vertical collection view section.
const CGFloat kVerticalListCellHeight = 80;
const CGFloat kVerticalListCellWidth = 343;

// The vertical spacing between cells.
const CGFloat kSpacingBetweenCells = 12;

// The vertical spacing below the header.
const CGFloat kSpacingBelowHeader = 16;

}  // namespace

@implementation HomeCustomizationCollectionConfigurator {
  // The customization page for the collection view.
  CustomizationMenuPage _page;
}

- (instancetype)initWithPage:(CustomizationMenuPage)page {
  self = [super init];
  if (self) {
    _page = page;
  }
  return self;
}

#pragma mark - Public

- (UICollectionViewLayout*)collectionViewLayout {
  UICollectionViewCompositionalLayoutConfiguration* configuration =
      [[UICollectionViewCompositionalLayoutConfiguration alloc] init];
  __weak __typeof(self) weakSelf = self;
  return [[UICollectionViewCompositionalLayout alloc]
      initWithSectionProvider:^(
          NSInteger sectionIndex,
          id<NSCollectionLayoutEnvironment> layoutEnvironment) {
        return [weakSelf sectionForIndex:sectionIndex
                       layoutEnvironment:layoutEnvironment];
      }
                configuration:configuration];
}

#pragma mark - Private

// Returns the section for a given `sectionIndex`.
- (NSCollectionLayoutSection*)
      sectionForIndex:(NSInteger)sectionIndex
    layoutEnvironment:(id<NSCollectionLayoutEnvironment>)layoutEnvironment {
  if ([self isVerticalListSection:sectionIndex]) {
    return [self verticalListSectionForLayoutEnvironment:layoutEnvironment];
  }
  return nil;
}

// Whether the current page should display a header.
- (BOOL)doesPageHaveHeader {
  return _page == CustomizationMenuPage::kDiscover ||
         _page == CustomizationMenuPage::kMagicStack;
}

// Whether the section at `sectionIndex` in the current diffable data source
// snapshot is a vertical list.
- (BOOL)isVerticalListSection:(NSInteger)sectionIndex {
  if (_page == CustomizationMenuPage::kMain) {
    return sectionIndex ==
           [_diffableDataSource.snapshot
               indexOfSectionIdentifier:kCustomizationSectionToggles];
  }

  if (_page == CustomizationMenuPage::kDiscover) {
    return sectionIndex ==
           [_diffableDataSource.snapshot
               indexOfSectionIdentifier:kCustomizationSectionDiscoverLinks];
  }

  return NO;
}

// Returns a section representing a vertical list of cells.
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
      (layoutEnvironment.container.contentSize.width - kVerticalListCellWidth) /
          2,
      0,
      (layoutEnvironment.container.contentSize.width - kVerticalListCellWidth) /
          2);

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

@end
