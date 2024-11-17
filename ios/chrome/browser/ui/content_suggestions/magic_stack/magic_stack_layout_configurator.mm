// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_layout_configurator.h"

#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_collection_view.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_utils.h"

@implementation MagicStackLayoutConfigurator {
  UICollectionViewCompositionalLayout* _magicStackLayout;
}

- (UICollectionViewCompositionalLayout*)magicStackCompositionalLayout {
  if (!_magicStackLayout) {
    UICollectionViewCompositionalLayoutConfiguration* config =
        [[UICollectionViewCompositionalLayoutConfiguration alloc] init];
      config.contentInsetsReference = UIContentInsetsReferenceNone;
    [config setScrollDirection:UICollectionViewScrollDirectionHorizontal];
    __weak MagicStackLayoutConfigurator* weakSelf = self;
    _magicStackLayout = [[UICollectionViewCompositionalLayout alloc]
        initWithSectionProvider:^(
            NSInteger sectionIndex,
            id<NSCollectionLayoutEnvironment> layoutEnvironment) {
          return [weakSelf sectionAtIndex:sectionIndex
                        layoutEnvironment:layoutEnvironment];
        }
                  configuration:config];
  }
  return _magicStackLayout;
}

#pragma mark - Private

- (NSCollectionLayoutSection*)sectionAtIndex:(NSInteger)sectionIndex
                           layoutEnvironment:(id<NSCollectionLayoutEnvironment>)
                                                 layoutEnvironment {
  NSCollectionLayoutDimension* itemWidthDimension =
      [NSCollectionLayoutDimension fractionalWidthDimension:1.];
  NSCollectionLayoutDimension* itemHeightDimension =
      [NSCollectionLayoutDimension fractionalHeightDimension:1.];
  NSCollectionLayoutSize* item_size =
      [NSCollectionLayoutSize sizeWithWidthDimension:itemWidthDimension
                                     heightDimension:itemHeightDimension];
  NSCollectionLayoutItem* item =
      [NSCollectionLayoutItem itemWithLayoutSize:item_size];

  CGSize size = layoutEnvironment.container.contentSize;
  CGFloat peekingInset = ModuleNarrowerWidthToAllowPeekingForTraitCollection(
      layoutEnvironment.traitCollection);
  if ([self.dataSource.snapshot
          numberOfItemsInSection:kMagicStackSectionIdentifier] == 1) {
    peekingInset = 0;
  }
  // Group size of fixed width for a module and height matching that of the
  // CollectionView.
  NSCollectionLayoutSize* group_size = [NSCollectionLayoutSize
      sizeWithWidthDimension:[NSCollectionLayoutDimension
                                 absoluteDimension:size.width - peekingInset]
             heightDimension:[NSCollectionLayoutDimension
                                 fractionalHeightDimension:1.]];
  if (sectionIndex ==
      [self.dataSource.snapshot
          indexOfSectionIdentifier:kMagicStackEditSectionIdentifier]) {
    // The edit button group should exactly match the horizontal spacing needed
    // for the cell's contents.
    group_size = [NSCollectionLayoutSize
        sizeWithWidthDimension:
            [NSCollectionLayoutDimension
                estimatedDimension:kMagicStackEditButtonWidth +
                                   kMagicStackEditButtonMargin * 2]
               heightDimension:[NSCollectionLayoutDimension
                                   fractionalHeightDimension:1.]];
  }

  NSCollectionLayoutGroup* group =
      [NSCollectionLayoutGroup horizontalGroupWithLayoutSize:group_size
                                                    subitems:@[ item ]];

  NSCollectionLayoutSection* section =
      [NSCollectionLayoutSection sectionWithGroup:group];
  section.interGroupSpacing = kMagicStackSpacing;
  return section;
}

@end
