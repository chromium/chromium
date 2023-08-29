// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_view.h"

@implementation TabResumptionView {
  // Item used to configure the view.
  TabResumptionItem* _item;
}

- (instancetype)initWithItem:(TabResumptionItem*)item {
  self = [super init];
  if (self) {
    _item = item;
  }
  return self;
}

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  [super willMoveToSuperview:newSuperview];

  [self createSubviews];
}

#pragma mark - Private methods

// Creates all the subviews
- (void)createSubviews {
  // TODO(crbug.com/1464185): Implement this.
}

@end
