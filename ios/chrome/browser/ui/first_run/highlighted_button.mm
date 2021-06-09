// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/highlighted_button.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Tha alpha to apply when the button is highlighted.
const CGFloat kHighlightedAlpha = 0.5;

// The duration of the animation when adding / removing transparency.
const CGFloat kAnimationDuration = 0.1;

}  // namespace

@implementation HighlightedButton

#pragma mark - Accessors

- (void)setHighlighted:(BOOL)highlighted {
  __weak __typeof(self) weakSelf = self;
  CGFloat targetAlpha = highlighted ? kHighlightedAlpha : 1.0;
  [UIView animateWithDuration:kAnimationDuration
                   animations:^{
                     weakSelf.alpha = targetAlpha;
                   }];
  [super setHighlighted:highlighted];
}

@end
