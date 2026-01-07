// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/elements/self_sizing_table_view.h"

@implementation SelfSizingTableView

- (void)setContentSize:(CGSize)contentSize {
  [super setContentSize:contentSize];
  [self invalidateIntrinsicContentSize];
}

- (CGSize)intrinsicContentSize {
  CGFloat height = self.contentSize.height;
  // Use directionalLayoutMargins instead of ContentInset, as the later is used
  // to inset the content above the keyboard.
  NSDirectionalEdgeInsets layoutMargins = self.directionalLayoutMargins;
  CGFloat viewHeight =
      (height == 0) ? 0 : height + layoutMargins.top + layoutMargins.bottom;

  return CGSizeMake(UIViewNoIntrinsicMetric, viewHeight);
}

@end
