// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/magic_stack/magic_stack_custom_layout.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/magic_stack/magic_stack_constants.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/magic_stack/magic_stack_layout_attributes.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/magic_stack/magic_stack_module_collection_view_cell.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_image_background_trait.h"

@implementation MagicStackCustomLayout

#pragma mark - UICollectionViewLayout

+ (Class)layoutAttributesClass {
  return [MagicStackLayoutAttributes class];
}

- (UICollectionViewDiffableDataSource*)diffableDataSource {
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

#pragma mark - Helpers

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
