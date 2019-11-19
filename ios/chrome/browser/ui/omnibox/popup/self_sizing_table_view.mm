// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/self_sizing_table_view.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation SelfSizingTableView

- (void)setContentSize:(CGSize)contentSize {
  [super setContentSize:contentSize];
  [self invalidateIntrinsicContentSize];
}

- (CGSize)intrinsicContentSize {
  CGFloat height = self.contentSize.height;
  CGFloat viewHeight =
      (height == 0) ? 0
                    : height + self.contentInset.top + self.contentInset.bottom;

  return CGSizeMake(UIViewNoIntrinsicMetric, viewHeight);
}

@end
