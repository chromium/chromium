// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/highlight_button.h"

#import "ios/chrome/common/material_timing.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation HighlightButton

- (void)setHighlighted:(BOOL)highlighted {
  [super setHighlighted:highlighted];

  NSArray<UIView*>* highlightViews;
  if (self.highlightableViews) {
    highlightViews = self.highlightableViews;
  } else {
    highlightViews = @[ self ];
  }

  [UIView transitionWithView:self
                    duration:ios::material::kDuration8
                     options:UIViewAnimationOptionCurveEaseInOut
                  animations:^{
                    for (UIView* view in highlightViews) {
                      view.alpha = highlighted ? 0.5 : 1.0;
                    }
                  }
                  completion:nil];
}

@end
