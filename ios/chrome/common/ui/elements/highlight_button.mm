// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/elements/highlight_button.h"

#import "ios/chrome/common/material_timing.h"

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
                    duration:kMaterialDuration8
                     options:UIViewAnimationOptionCurveEaseInOut
                  animations:^{
                    for (UIView* view in highlightViews) {
                      view.alpha = highlighted ? 0.5 : 1.0;
                    }
                  }
                  completion:nil];
}

@end
