// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_bar/ui/app_bar_view.h"

@implementation AppBarView

#pragma mark - UIView

- (void)didMoveToWindow {
  [super didMoveToWindow];
  [self.delegate appBarViewDidMoveToWindow:self];
}

@end
