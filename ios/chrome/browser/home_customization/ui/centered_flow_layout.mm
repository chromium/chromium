// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/centered_flow_layout.h"

@implementation CenteredFlowLayout

- (NSArray<UICollectionViewLayoutAttributes*>*)
    layoutAttributesForElementsInRect:(CGRect)rect {
  NSArray<UICollectionViewLayoutAttributes*>* originalAttributes =
      [super layoutAttributesForElementsInRect:rect];
  NSMutableArray<UICollectionViewLayoutAttributes*>* updatedAttributes =
      [[NSMutableArray alloc] init];

  CGFloat contentWidth = self.collectionView.bounds.size.width -
                         self.sectionInset.left - self.sectionInset.right;

  NSMutableDictionary<
      NSNumber*, NSMutableArray<UICollectionViewLayoutAttributes*>*>* rows =
      [NSMutableDictionary dictionary];

  // Group attributes by row based on Y position (the vertical center of the
  // row).
  for (UICollectionViewLayoutAttributes* attributes in originalAttributes) {
    if (attributes.representedElementCategory ==
        UICollectionElementCategoryCell) {
      CGFloat centerY = CGRectGetMidY(attributes.frame);
      NSNumber* rowKey = @(centerY);
      if (!rows[rowKey]) {
        rows[rowKey] = [NSMutableArray array];
      }
      [rows[rowKey] addObject:[attributes copy]];
    } else {
      [updatedAttributes
          addObject:[attributes copy]];  // Headers, footers, etc.
    }
  }

  // Adjust frames for each row.
  for (NSNumber* key in rows) {
    NSMutableArray<UICollectionViewLayoutAttributes*>* rowAttributes =
        rows[key];
    if (rowAttributes.count == 0) {
      continue;
    }

    CGFloat interitemSpacing = self.minimumInteritemSpacing;

    // Calculate the total width of the row by finding the min and max X
    // coordinates of the items.
    CGFloat minX = CGFLOAT_MAX;
    CGFloat maxX = CGFLOAT_MIN;
    for (UICollectionViewLayoutAttributes* attr in rowAttributes) {
      minX = MIN(minX, CGRectGetMinX(attr.frame));
      maxX = MAX(maxX, CGRectGetMaxX(attr.frame));
    }
    CGFloat totalRowWidth = maxX - minX;

    // If the whole width is not taken, the custom sorting logic is used (i.e.,
    // simply add an offset to the X coord).
    if (totalRowWidth < contentWidth) {
      CGFloat offset = (contentWidth - totalRowWidth) / 2.0;
      CGFloat currentX = self.sectionInset.left + offset;

      // Sort attributes by their original X position to maintain order.
      [rowAttributes sortUsingComparator:^NSComparisonResult(
                         UICollectionViewLayoutAttributes* obj1,
                         UICollectionViewLayoutAttributes* obj2) {
        return [@(obj1.frame.origin.x) compare:@(obj2.frame.origin.x)];
      }];

      for (UICollectionViewLayoutAttributes* attr in rowAttributes) {
        CGRect frame = attr.frame;
        frame.origin.x = currentX;
        attr.frame = frame;
        currentX += frame.size.width + interitemSpacing;
      }
    }
    [updatedAttributes addObjectsFromArray:rowAttributes];
  }

  return updatedAttributes;
}

@end
