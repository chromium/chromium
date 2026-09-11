// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/magic_stack/ui/magic_stack_smart_stack_layout.h"

#import <algorithm>
#import <cmath>

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/content_suggestions/magic_stack/public/magic_stack_constants.h"
#import "ios/chrome/browser/content_suggestions/magic_stack/public/magic_stack_utils.h"
#import "ios/chrome/browser/content_suggestions/magic_stack/ui/edit_button_config.h"
#import "ios/chrome/browser/content_suggestions/magic_stack/ui/magic_stack_collection_view.h"
#import "ios/chrome/browser/content_suggestions/magic_stack/ui/magic_stack_layout_attributes.h"
#import "ios/chrome/browser/content_suggestions/magic_stack/ui/magic_stack_module_collection_view_cell.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_image_background_trait.h"

@implementation MagicStackSmartStackLayout

- (instancetype)init {
  self = [super init];
  if (self) {
    self.scrollDirection = UICollectionViewScrollDirectionHorizontal;
    self.minimumLineSpacing = kMagicStackSpacing;
  }
  return self;
}

+ (Class)layoutAttributesClass {
  return [MagicStackLayoutAttributes class];
}

- (void)prepareLayout {
  [super prepareLayout];

  CGSize boundsSize = self.collectionView.bounds.size;
  CGFloat peekingInset =
      [self peekingInsetForTraitCollection:self.collectionView.traitCollection
                                viewBounds:self.collectionView.bounds];
  CGFloat cardWidth =
      [self cardWidthForTraitCollection:self.collectionView.traitCollection
                             viewBounds:self.collectionView.bounds];
  CGFloat cardHeight = boundsSize.height;
  if (cardHeight <= 0) {
    cardHeight = GetMagicStackHeight(self.collectionView);
  }
  self.itemSize = CGSizeMake(cardWidth, cardHeight);

  CGFloat horizontalInset = peekingInset / 2.0;
  self.sectionInset = UIEdgeInsetsMake(0, horizontalInset, 0, horizontalInset);
}

- (NSArray<UICollectionViewLayoutAttributes*>*)
    layoutAttributesForElementsInRect:(CGRect)rect {
  NSArray<UICollectionViewLayoutAttributes*>* attributesArray =
      [super layoutAttributesForElementsInRect:rect];
  NSMutableArray<UICollectionViewLayoutAttributes*>* copiedAttributes =
      [[NSMutableArray alloc] init];

  CGSize boundsSize = self.collectionView.bounds.size;
  CGFloat centerX =
      self.collectionView.contentOffset.x + boundsSize.width / 2.0;

  for (UICollectionViewLayoutAttributes* attributes in attributesArray) {
    UICollectionViewLayoutAttributes* copied = [attributes copy];
    if (copied.representedElementCategory == UICollectionElementCategoryCell) {
      [self apply3DTransformToAttributes:base::apple::ObjCCastStrict<
                                             MagicStackLayoutAttributes>(copied)
                                 centerX:centerX];
    }
    [copiedAttributes addObject:copied];
  }
  return copiedAttributes;
}

- (UICollectionViewLayoutAttributes*)layoutAttributesForItemAtIndexPath:
    (NSIndexPath*)indexPath {
  UICollectionViewLayoutAttributes* attributes =
      [[super layoutAttributesForItemAtIndexPath:indexPath] copy];
  if (attributes.representedElementCategory ==
      UICollectionElementCategoryCell) {
    CGSize boundsSize = self.collectionView.bounds.size;
    CGFloat centerX =
        self.collectionView.contentOffset.x + boundsSize.width / 2.0;
    [self
        apply3DTransformToAttributes:base::apple::ObjCCastStrict<
                                         MagicStackLayoutAttributes>(attributes)
                             centerX:centerX];
  }
  return attributes;
}

- (void)apply3DTransformToAttributes:(MagicStackLayoutAttributes*)attributes
                             centerX:(CGFloat)centerX {
  attributes.subviewAlpha = 1.0;

  CGFloat distance = attributes.center.x - centerX;
  CGFloat activeDistance = self.itemSize.width + self.minimumLineSpacing;

  CGFloat normalizedDistance = distance / activeDistance;
  normalizedDistance = MAX(-1.0, MIN(1.0, normalizedDistance));

  // Active Card: Scale = 1.0; Stacked Card: Scale = 0.85
  CGFloat scale = 1.0 - 0.15 * std::abs(normalizedDistance);

  CATransform3D transform = CATransform3DIdentity;
  transform.m34 = -1.0 / 500.0;

  // Apply Y-axis rotation to create a 3D horizontal roll/stack transition
  CGFloat maxRotationAngle = 15.0 * M_PI / 180.0;
  CGFloat rotationAngle = -normalizedDistance * maxRotationAngle;
  transform = CATransform3DRotate(transform, rotationAngle, 0.0, 1.0, 0.0);

  // Apply scaling
  transform = CATransform3DScale(transform, scale, scale, 1.0);

  attributes.transform3D = transform;
  attributes.zIndex =
      static_cast<int>(100.0 * (1.0 - std::abs(normalizedDistance)));
}

- (BOOL)shouldInvalidateLayoutForBoundsChange:(CGRect)newBounds {
  return YES;
}

- (CGPoint)targetContentOffsetForProposedContentOffset:
               (CGPoint)proposedContentOffset
                                 withScrollingVelocity:(CGPoint)velocity {
  CGFloat centerX =
      proposedContentOffset.x + self.collectionView.bounds.size.width / 2.0;
  NSArray<UICollectionViewLayoutAttributes*>* attributesArray = [self
      layoutAttributesForElementsInRect:CGRectMake(proposedContentOffset.x, 0,
                                                   self.collectionView.bounds
                                                       .size.width,
                                                   self.collectionView.bounds
                                                       .size.height)];

  CGFloat minDistance = CGFLOAT_MAX;
  CGFloat offsetAdjustment = 0;

  for (UICollectionViewLayoutAttributes* attributes in attributesArray) {
    if (attributes.representedElementCategory ==
        UICollectionElementCategoryCell) {
      CGFloat distance = attributes.center.x - centerX;
      if (std::abs(distance) < std::abs(minDistance)) {
        minDistance = distance;
        offsetAdjustment = distance;
      }
    }
  }

  return CGPointMake(proposedContentOffset.x + offsetAdjustment,
                     proposedContentOffset.y);
}

- (UICollectionViewLayoutAttributes*)
    finalLayoutAttributesForDisappearingItemAtIndexPath:
        (NSIndexPath*)indexPath {
  MagicStackLayoutAttributes* attributes =
      base::apple::ObjCCast<MagicStackLayoutAttributes>([super
          finalLayoutAttributesForDisappearingItemAtIndexPath:indexPath]);

  if (attributes) {
    attributes.subviewAlpha = 0;
    if ([self indexPathHasBlurredBackground:attributes.indexPath]) {
      attributes.alpha = 1;
    }
  }

  return attributes;
}

- (UICollectionViewLayoutAttributes*)
    initialLayoutAttributesForAppearingItemAtIndexPath:(NSIndexPath*)indexPath {
  MagicStackLayoutAttributes* attributes =
      base::apple::ObjCCast<MagicStackLayoutAttributes>(
          [super initialLayoutAttributesForAppearingItemAtIndexPath:indexPath]);

  if (attributes) {
    attributes.subviewAlpha = 0;
    if ([self indexPathHasBlurredBackground:attributes.indexPath]) {
      attributes.alpha = 1;
    }
  }

  return attributes;
}

- (UICollectionViewDiffableDataSource*)diffableDataSource {
  return base::apple::ObjCCast<UICollectionViewDiffableDataSource>(
      self.collectionView.dataSource);
}

#pragma mark - Helpers

- (BOOL)indexPathHasBlurredBackground:(NSIndexPath*)indexPath {
  if (![self.collectionView
              .traitCollection boolForNewTabPageImageBackgroundTrait]) {
    return NO;
  }
  MagicStackModule* item =
      [self.diffableDataSource itemIdentifierForIndexPath:indexPath];
  return item != nil && ![item isKindOfClass:[EditButtonConfig class]];
}

#pragma mark - MagicStackPagingLayoutProvider

- (CGFloat)offsetForPage:(NSUInteger)page
         traitCollection:(UITraitCollection*)traitCollection
              viewBounds:(CGRect)bounds {
  CGFloat peekingInset = [self peekingInsetForTraitCollection:traitCollection
                                                   viewBounds:bounds];
  CGFloat cardWidth = [self cardWidthForTraitCollection:traitCollection
                                             viewBounds:bounds];
  CGFloat horizontalInset = peekingInset / 2.0;
  CGFloat itemX =
      horizontalInset + page * (cardWidth + self.minimumLineSpacing);
  CGFloat itemCenter = itemX + cardWidth / 2.0;
  CGFloat targetOffset = itemCenter - bounds.size.width / 2.0;
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
  CGFloat cardWidth = [self cardWidthForTraitCollection:traitCollection
                                             viewBounds:bounds];
  NSUInteger totalItems = [self totalItemCount];
  NSUInteger targetPage =
      MagicStackTargetPage(currentOffset, velocity, cardWidth, totalItems);
  *pageIndex = targetPage;

  return [self offsetForPage:*pageIndex
             traitCollection:traitCollection
                  viewBounds:bounds];
}

#pragma mark - Helpers

// Returns the number of items in the modules section.
- (NSInteger)numberOfModules {
  if (self.collectionView && [self.collectionView numberOfSections] > 0) {
    return [self.collectionView numberOfItemsInSection:0];
  }
  return 0;
}

// Returns the total number of items across all sections.
- (NSUInteger)totalItemCount {
  if (self.diffableDataSource) {
    return self.diffableDataSource.snapshot.numberOfItems;
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

// Returns the peeking inset for the Magic Stack based on trait collection,
// bounds width, and number of items.
- (CGFloat)peekingInsetForTraitCollection:(UITraitCollection*)traitCollection
                               viewBounds:(CGRect)bounds {
  CGFloat peekingInset = MagicStackModuleNarrowerWidthToAllowPeeking(
      traitCollection, bounds.size.width);
  if ([self numberOfModules] == 1) {
    return 0;
  }
  return peekingInset;
}

// Returns the card width for the Magic Stack.
- (CGFloat)cardWidthForTraitCollection:(UITraitCollection*)traitCollection
                            viewBounds:(CGRect)bounds {
  return bounds.size.width - [self
                                 peekingInsetForTraitCollection:traitCollection
                                                     viewBounds:bounds];
}

@end
