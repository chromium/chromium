// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui/toolbar_view.h"

@implementation ToolbarView

#pragma mark - UIView

- (void)didMoveToWindow {
  [super didMoveToWindow];
  [self.delegate toolbarViewDidMoveToWindow:self];
}

@end
