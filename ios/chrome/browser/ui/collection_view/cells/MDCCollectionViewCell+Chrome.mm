// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation MDCCollectionViewCell (Chrome)

static NSMutableDictionary<NSString*, MDCCollectionViewCell*>*
    gSharedSizingCells;

+ (void)cr_clearPreferredHeightForWidthCellCache {
  [gSharedSizingCells removeAllObjects];
}

+ (CGFloat)cr_preferredHeightForWidth:(CGFloat)targetWidth
                              forItem:(CollectionViewItem*)item {
  // Dictionary where keys are class names and values are sizing cells.
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    gSharedSizingCells = [NSMutableDictionary dictionary];
  });

  // Get the sizing cell for the given class, or create it if needed.
  NSString* className = NSStringFromClass(item.cellClass);
  MDCCollectionViewCell* cell = gSharedSizingCells[className];
  if (!cell) {
    cell = [[item.cellClass alloc] init];
    // Don't use autoresizing mask constraints, as they will conflict when
    // laying out the cell.
    cell.contentView.translatesAutoresizingMaskIntoConstraints = NO;
    // Store it in the dictionary.
    gSharedSizingCells[className] = cell;
  }

  // Configure the cell with the item.
  [item configureCell:cell];
  return [cell cr_preferredHeightForWidth:targetWidth];
}

- (CGFloat)cr_preferredHeightForWidth:(CGFloat)targetWidth {
  // Set the cell's frame to use the target width.
  CGRect frame = self.frame;
  const CGFloat kMaxHeight = 0;
  frame.size = CGSizeMake(targetWidth, kMaxHeight);
  self.frame = frame;

  // Layout the cell.
  [self setNeedsLayout];
  [self layoutIfNeeded];

  // Compute how tall the cell needs to be.
  CGSize computedSize =
      [self systemLayoutSizeFittingSize:UILayoutFittingCompressedSize];

  return computedSize.height;
}

- (void)cr_setAccessoryType:(MDCCollectionViewCellAccessoryType)accessoryType {
  self.accessoryType = accessoryType;
  if (accessoryType == MDCCollectionViewCellAccessoryDisclosureIndicator)
    self.accessoryView.tintColor = [UIColor colorNamed:kGrey400Color];
}

@end
