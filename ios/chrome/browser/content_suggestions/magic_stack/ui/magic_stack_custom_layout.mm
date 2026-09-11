// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/magic_stack/ui/magic_stack_custom_layout.h"

#import <algorithm>
#import <cmath>

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/content_suggestions/magic_stack/public/magic_stack_constants.h"
#import "ios/chrome/browser/content_suggestions/magic_stack/public/magic_stack_utils.h"
#import "ios/chrome/browser/content_suggestions/magic_stack/ui/magic_stack_layout_attributes.h"
#import "ios/chrome/browser/content_suggestions/magic_stack/ui/magic_stack_module_collection_view_cell.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_image_background_trait.h"

@implementation MagicStackCustomLayout

#pragma mark - Initialization

- (instancetype)init {
  UICollectionViewCompositionalLayoutConfiguration* config =
      [[UICollectionViewCompositionalLayoutConfiguration alloc] init];
  config.contentInsetsReference = UIContentInsetsReferenceNone;
  config.scrollDirection = UICollectionViewScrollDirectionHorizontal;
  __weak __typeof(self) weakSelf = self;
  self = [super
      initWithSectionProvider:^(
          NSInteger sectionIndex,
          id<NSCollectionLayoutEnvironment> layoutEnvironment) {
        return [weakSelf sectionAtIndex:sectionIndex
                      layoutEnvironment:layoutEnvironment];
      }
                configuration:config];
  return self;
}

#pragma mark - Section Configuration

- (NSCollectionLayoutSection*)sectionAtIndex:(NSInteger)sectionIndex
                           layoutEnvironment:(id<NSCollectionLayoutEnvironment>)
                                                 layoutEnvironment {
  NSCollectionLayoutDimension* itemWidthDimension =
      [NSCollectionLayoutDimension fractionalWidthDimension:1.];
  NSCollectionLayoutDimension* itemHeightDimension =
      [NSCollectionLayoutDimension fractionalHeightDimension:1.];
  NSCollectionLayoutSize* itemSize =
      [NSCollectionLayoutSize sizeWithWidthDimension:itemWidthDimension
                                     heightDimension:itemHeightDimension];
  NSCollectionLayoutItem* item =
      [NSCollectionLayoutItem itemWithLayoutSize:itemSize];

  CGSize containerSize = layoutEnvironment.container.contentSize;
  CGFloat moduleWidth =
      [self moduleWidthForTraitCollection:layoutEnvironment.traitCollection
                               viewBounds:CGRectMake(0, 0, containerSize.width,
                                                     containerSize.height)];

  // Group size of fixed width for a module and height matching that of the
  // CollectionView.
  NSCollectionLayoutSize* groupSize = [NSCollectionLayoutSize
      sizeWithWidthDimension:[NSCollectionLayoutDimension
                                 absoluteDimension:moduleWidth]
             heightDimension:[NSCollectionLayoutDimension
                                 fractionalHeightDimension:1.]];

  NSInteger editSectionIndex =
      self.dataSource
          ? [self.dataSource.snapshot
                indexOfSectionIdentifier:kMagicStackEditSectionIdentifier]
          : NSNotFound;
  if (editSectionIndex != NSNotFound && sectionIndex == editSectionIndex) {
    // The edit button group should exactly match the horizontal spacing needed
    // for the cell's contents.
    groupSize = [NSCollectionLayoutSize
        sizeWithWidthDimension:
            [NSCollectionLayoutDimension
                estimatedDimension:kMagicStackEditButtonWidth +
                                   kMagicStackEditButtonMargin * 2]
               heightDimension:[NSCollectionLayoutDimension
                                   fractionalHeightDimension:1.]];
  }

  NSCollectionLayoutGroup* group =
      [NSCollectionLayoutGroup horizontalGroupWithLayoutSize:groupSize
                                                    subitems:@[ item ]];

  NSCollectionLayoutSection* section =
      [NSCollectionLayoutSection sectionWithGroup:group];
  section.interGroupSpacing = kMagicStackSpacing;
  return section;
}

#pragma mark - UICollectionViewLayout

+ (Class)layoutAttributesClass {
  return [MagicStackLayoutAttributes class];
}

- (UICollectionViewDiffableDataSource*)dataSource {
  return base::apple::ObjCCast<UICollectionViewDiffableDataSource>(
      self.collectionView.dataSource);
}

- (NSArray<UICollectionViewLayoutAttributes*>*)
    layoutAttributesForElementsInRect:(CGRect)rect {
  NSArray<UICollectionViewLayoutAttributes*>* attributesArray =
      [super layoutAttributesForElementsInRect:rect];
  for (UICollectionViewLayoutAttributes* attributes : attributesArray) {
    MagicStackLayoutAttributes* typedAttributes =
        base::apple::ObjCCast<MagicStackLayoutAttributes>(attributes);
    typedAttributes.subviewAlpha = 1;
  }

  return attributesArray;
}

- (UICollectionViewLayoutAttributes*)layoutAttributesForItemAtIndexPath:
    (NSIndexPath*)indexPath {
  MagicStackLayoutAttributes* attributes =
      base::apple::ObjCCast<MagicStackLayoutAttributes>(
          [super layoutAttributesForItemAtIndexPath:indexPath]);

  attributes.subviewAlpha = 1;

  return attributes;
}

- (UICollectionViewLayoutAttributes*)
    finalLayoutAttributesForDisappearingItemAtIndexPath:
        (NSIndexPath*)indexPath {
  MagicStackLayoutAttributes* attributes =
      base::apple::ObjCCast<MagicStackLayoutAttributes>([super
          finalLayoutAttributesForDisappearingItemAtIndexPath:indexPath]);

  attributes.subviewAlpha = 0;

  if ([self indexPathHasBlurredBackground:attributes.indexPath]) {
    // Do not use alpha when cell has blurred background. UIVisualEffectView
    // cannot handle alpha != 1. The cell will handle animating its subviews
    // using subviewAlpha.
    attributes.alpha = 1;
  }

  return attributes;
}

- (UICollectionViewLayoutAttributes*)
    initialLayoutAttributesForAppearingItemAtIndexPath:(NSIndexPath*)indexPath {
  MagicStackLayoutAttributes* attributes =
      base::apple::ObjCCast<MagicStackLayoutAttributes>(
          [super initialLayoutAttributesForAppearingItemAtIndexPath:indexPath]);

  attributes.subviewAlpha = 0;

  if ([self indexPathHasBlurredBackground:attributes.indexPath]) {
    // Do not use alpha when cell has blurred background. UIVisualEffectView
    // cannot handle alpha != 1. The cell will handle animating its subviews
    // using subviewAlpha.
    attributes.alpha = 1;
  }

  return attributes;
}

#pragma mark - MagicStackPagingLayoutProvider

- (CGFloat)offsetForPage:(NSUInteger)page
         traitCollection:(UITraitCollection*)traitCollection
              viewBounds:(CGRect)bounds {
  CGFloat moduleWidth = [self moduleWidthForTraitCollection:traitCollection
                                                 viewBounds:bounds];
  CGFloat peekOffset = [self peekOffsetForPage:page
                               traitCollection:traitCollection
                                    viewBounds:bounds];

  CGFloat targetOffset = page * (moduleWidth + kMagicStackSpacing) - peekOffset;
  if (self.collectionView &&
      self.collectionView.contentSize.width > bounds.size.width) {
    CGFloat maxOffset =
        self.collectionView.contentSize.width - bounds.size.width;
    targetOffset = std::min(targetOffset, maxOffset);
  }
  return MAX(0, targetOffset);
}

- (CGFloat)targetPageOffsetForOffset:(CGFloat)currentOffset
                            velocity:(CGFloat)velocity
                     traitCollection:(UITraitCollection*)traitCollection
                          viewBounds:(CGRect)bounds
                         currentPage:(NSUInteger*)pageIndex {
  CGFloat moduleWidth = [self moduleWidthForTraitCollection:traitCollection
                                                 viewBounds:bounds];
  NSUInteger totalItems = [self totalItemCount];
  NSUInteger targetPage =
      MagicStackTargetPage(currentOffset, velocity, moduleWidth, totalItems);
  *pageIndex = targetPage;

  return [self offsetForPage:*pageIndex
             traitCollection:traitCollection
                  viewBounds:bounds];
}

#pragma mark - Helpers

// Returns the number of items in the modules section.
- (NSInteger)numberOfModules {
  if (self.dataSource) {
    return [self.dataSource.snapshot
        numberOfItemsInSection:kMagicStackSectionIdentifier];
  }
  if (self.collectionView && [self.collectionView numberOfSections] > 0) {
    return [self.collectionView numberOfItemsInSection:0];
  }
  return 0;
}

// Returns the total number of items across all sections.
- (NSUInteger)totalItemCount {
  if (self.dataSource) {
    return self.dataSource.snapshot.numberOfItems;
  }
  if (!self.collectionView) {
    return 0;
  }
  NSInteger count = 0;
  for (NSInteger s = 0; s < self.collectionView.numberOfSections; s++) {
    count += [self.collectionView numberOfItemsInSection:s];
  }
  return static_cast<NSUInteger>(count);
}

// Returns the effective width of a module for the given trait collection and
// bounds.
- (CGFloat)moduleWidthForTraitCollection:(UITraitCollection*)traitCollection
                              viewBounds:(CGRect)bounds {
  CGFloat peekingInset = MagicStackModuleNarrowerWidthToAllowPeeking(
      traitCollection, bounds.size.width);
  if ([self numberOfModules] <= 1) {
    peekingInset = 0;
  }
  return bounds.size.width - peekingInset;
}

// Returns the trailing peek offset for a given page index.
- (CGFloat)peekOffsetForPage:(NSUInteger)page
             traitCollection:(UITraitCollection*)traitCollection
                  viewBounds:(CGRect)bounds {
  NSInteger numberOfItems = [self numberOfModules];
  // If there's only one module, no peek offset is needed.
  if (numberOfItems > 1 && page == static_cast<NSUInteger>(numberOfItems - 1)) {
    // The last module should be trailing aligned so the previous module peeks.
    BOOL shouldHaveWideLayout =
        ShouldMagicStackHaveWideLayout(traitCollection, bounds.size.width);
    return shouldHaveWideLayout ? kMagicStackPeekInsetLandscape
                                : kMagicStackPeekInset + 1;
  }
  return 0;
}

// Whether the given index path has a blurred background. If it does not, the
// default layout attributes can be used.
- (BOOL)indexPathHasBlurredBackground:(NSIndexPath*)indexPath {
  if (![self.collectionView
              .traitCollection boolForNewTabPageImageBackgroundTrait]) {
    return NO;
  }
  return [[self.collectionView cellForItemAtIndexPath:indexPath]
      isKindOfClass:[MagicStackModuleCollectionViewCell class]];
}

@end
