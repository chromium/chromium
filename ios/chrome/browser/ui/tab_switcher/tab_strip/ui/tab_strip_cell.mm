// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/ui/tab_strip_cell.h"

@implementation TabStripCell

#pragma mark - UICollectionViewCell

- (void)prepareForReuse {
  [super prepareForReuse];
  self.title = nil;
  self.groupStrokeColor = nil;
  self.intersectsLeftEdge = NO;
  self.intersectsRightEdge = NO;
}

@end
