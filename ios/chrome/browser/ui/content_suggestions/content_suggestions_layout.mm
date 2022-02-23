// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_layout.h"

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_omnibox_positioning.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_utils.h"
#import "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ContentSuggestionsLayout

- (CGFloat)minimumNTPHeight {
  CGFloat collectionViewHeight = self.parentCollectionView.bounds.size.height;
  CGFloat headerHeight = [self firstHeaderHeight];

  // The minimum height for the collection view content should be the height of
  // the header plus the height of the collection view minus the height of the
  // NTP bottom bar. This allows the Most Visited cells to be scrolled up to the
  // top of the screen. Also computes the total NTP scrolling height for
  // Discover infinite feed.
  CGFloat ntpHeight = collectionViewHeight + headerHeight;
  CGFloat minimumHeight =
      ntpHeight - ntp_header::kScrolledToTopOmniboxBottomMargin;
  if (!IsRegularXRegularSizeClass(self.collectionView)) {
    CGFloat toolbarHeight =
        IsSplitToolbarMode(self.collectionView)
            ? ToolbarExpandedHeight([UIApplication sharedApplication]
                                        .preferredContentSizeCategory)
            : 0;
    CGFloat additionalHeight =
        toolbarHeight + self.collectionView.contentInset.bottom;
    minimumHeight -= additionalHeight;
  }

  return minimumHeight;
}

- (NSArray*)layoutAttributesForElementsInRect:(CGRect)rect {
  if (IsRegularXRegularSizeClass(self.collectionView) ||
      IsContentSuggestionsHeaderMigrationEnabled())
    return [super layoutAttributesForElementsInRect:rect];

  NSMutableArray* layoutAttributes =
      [[super layoutAttributesForElementsInRect:rect] mutableCopy];
  UICollectionViewLayoutAttributes* fixedHeaderAttributes = nil;
  NSIndexPath* fixedHeaderIndexPath =
      [NSIndexPath indexPathForItem:0 inSection:0];

  for (UICollectionViewLayoutAttributes* attributes in layoutAttributes) {
    if ([attributes.representedElementKind
            isEqualToString:UICollectionElementKindSectionHeader] &&
        attributes.indexPath.section == fixedHeaderIndexPath.section) {
      fixedHeaderAttributes = [self
          layoutAttributesForSupplementaryViewOfKind:
              UICollectionElementKindSectionHeader
                                         atIndexPath:fixedHeaderIndexPath];
      attributes.zIndex = fixedHeaderAttributes.zIndex;
      attributes.frame = fixedHeaderAttributes.frame;
    }
  }

  // The fixed header's attributes are not updated if the header's default frame
  // is far enough away from |rect|, which can occur when the NTP is scrolled
  // up.
  if (!fixedHeaderAttributes) {
    UICollectionViewLayoutAttributes* fixedHeaderAttributes =
        [self layoutAttributesForSupplementaryViewOfKind:
                  UICollectionElementKindSectionHeader
                                             atIndexPath:fixedHeaderIndexPath];
    [layoutAttributes addObject:fixedHeaderAttributes];
  }

  return layoutAttributes;
}

- (UICollectionViewLayoutAttributes*)
layoutAttributesForSupplementaryViewOfKind:(NSString*)kind
                               atIndexPath:(NSIndexPath*)indexPath {
  UICollectionViewLayoutAttributes* attributes =
      [super layoutAttributesForSupplementaryViewOfKind:kind
                                            atIndexPath:indexPath];
  if (!IsSplitToolbarMode(self.collectionView) ||
      IsContentSuggestionsHeaderMigrationEnabled())
    return attributes;

  if ([kind isEqualToString:UICollectionElementKindSectionHeader] &&
      indexPath.section == 0) {
    CGFloat contentOffset = self.parentCollectionView.contentOffset.y +
                            self.collectionView.contentSize.height;

    CGFloat headerHeight = CGRectGetHeight(attributes.frame);
    CGPoint origin = attributes.frame.origin;

    // Keep the header in front of all other views.
    attributes.zIndex = NSIntegerMax;

    // TODO(crbug.com/1114792): Remove this and only use omniboxPositioner after
    // refactoring is complete.
    CGFloat minY =
        headerHeight - ntp_header::kFakeOmniboxScrolledToTopMargin -
        ToolbarExpandedHeight(
            [UIApplication sharedApplication].preferredContentSizeCategory) -
        self.collectionView.safeAreaInsets.top;

    minY = [self.omniboxPositioner stickyOmniboxHeight];
    // TODO(crbug.com/1114792): Remove mentioned of "refactored" from the
    // variable name once this launches.
    BOOL hasScrolledIntoRefactoredDiscoverFeed = self.isScrolledIntoFeed;
    if (contentOffset > minY && !hasScrolledIntoRefactoredDiscoverFeed) {
      origin.y = contentOffset - minY;
    }
    attributes.frame = {origin, attributes.frame.size};
  }
  return attributes;
}

- (BOOL)shouldInvalidateLayoutForBoundsChange:(CGRect)newBound {
  if (IsRegularXRegularSizeClass(self.collectionView))
    return [super shouldInvalidateLayoutForBoundsChange:newBound];
  return YES;
}

#pragma mark - MDCCollectionViewFlowLayout overrides
// This section contains overrides of methods to avoid ugly effects during
// rotation due to the default behavior of the MDCCollectionViewFlowLayout. See
// http://crbug.com/949659 .

- (UICollectionViewLayoutAttributes*)
    initialLayoutAttributesForAppearingItemAtIndexPath:
        (NSIndexPath*)itemIndexPath {
  UICollectionViewLayoutAttributes* attribute =
      [super initialLayoutAttributesForAppearingItemAtIndexPath:itemIndexPath];
  attribute.alpha = 0;
  return attribute;
}

- (UICollectionViewLayoutAttributes*)
    initialLayoutAttributesForAppearingSupplementaryElementOfKind:
        (NSString*)elementKind
                                                      atIndexPath:
                                                          (NSIndexPath*)
                                                              elementIndexPath {
  UICollectionViewLayoutAttributes* attribute = [super
      initialLayoutAttributesForAppearingSupplementaryElementOfKind:elementKind
                                                        atIndexPath:
                                                            elementIndexPath];
  attribute.alpha = 0;
  return attribute;
}

- (UICollectionViewLayoutAttributes*)
    finalLayoutAttributesForDisappearingItemAtIndexPath:
        (NSIndexPath*)itemIndexPath {
  UICollectionViewLayoutAttributes* attribute =
      [super finalLayoutAttributesForDisappearingItemAtIndexPath:itemIndexPath];
  attribute.alpha = 0;
  return attribute;
}

- (UICollectionViewLayoutAttributes*)
    finalLayoutAttributesForDisappearingSupplementaryElementOfKind:
        (NSString*)elementKind
                                                       atIndexPath:
                                                           (NSIndexPath*)
                                                               indexPath {
  UICollectionViewLayoutAttributes* attribute = [super
      finalLayoutAttributesForDisappearingSupplementaryElementOfKind:elementKind
                                                         atIndexPath:indexPath];
  attribute.alpha = 0;
  return attribute;
}

#pragma mark - Private.

// Returns the height of the header of the first section.
- (CGFloat)firstHeaderHeight {
  id<UICollectionViewDelegateFlowLayout> delegate =
      static_cast<id<UICollectionViewDelegateFlowLayout>>(
          self.collectionView.delegate);
  return [delegate collectionView:self.collectionView
                                      layout:self
             referenceSizeForHeaderInSection:0]
      .height;
}

@end
