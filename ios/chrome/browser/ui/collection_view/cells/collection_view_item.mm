// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"

#import <MaterialComponents/MaterialCollectionCells.h>

#import "base/check.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CollectionViewItem

- (instancetype)initWithType:(NSInteger)type {
  if ((self = [super initWithType:type])) {
    self.cellClass = [MDCCollectionViewCell class];
  }
  return self;
}

- (void)configureCell:(MDCCollectionViewCell*)cell {
  DCHECK([cell class] == self.cellClass);
  cell.accessibilityTraits = self.accessibilityTraits;
  cell.accessibilityIdentifier = self.accessibilityIdentifier;
}

@end
