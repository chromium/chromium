// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/pinned_tabs/pinned_tabs_layout.h"

#import "ios/chrome/browser/ui/tab_switcher/pinned_tabs/pinned_tabs_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation PinnedTabsLayout

- (instancetype)init {
  if (self = [super init]) {
    self.scrollDirection = UICollectionViewScrollDirectionHorizontal;
  }
  return self;
}

#pragma mark - UICollectionViewLayout

- (void)prepareLayout {
  [super prepareLayout];

  self.itemSize = CGSize{kPinnedCelldWidth, kPinnedCelldHeight};

  self.sectionInset = UIEdgeInsets{
      kPinnedCellVerticalLayoutInsets, kPinnedCellHorizontalLayoutInsets,
      kPinnedCellVerticalLayoutInsets, kPinnedCellHorizontalLayoutInsets};
}

@end
