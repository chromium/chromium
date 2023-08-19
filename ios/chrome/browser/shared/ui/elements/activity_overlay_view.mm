// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/elements/activity_overlay_view.h"

#import "ios/chrome/browser/shared/ui/elements/elements_constants.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation ActivityOverlayView

- (instancetype)init {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _indicator = GetLargeUIActivityIndicatorView();
    _indicator.translatesAutoresizingMaskIntoConstraints = NO;
  }
  return self;
}

- (void)didMoveToSuperview {
  if (self.subviews.count == 0) {
    // This is the first time the view is used, finish setting everything up.
    self.accessibilityIdentifier = kActivityOverlayViewAccessibilityIdentifier;
    [self addSubview:self.indicator];
    AddSameCenterConstraints(self, self.indicator);
    [self.indicator startAnimating];
    // It is better to use background color instead of alpha, so
    // ActivityOverlayView blocks all taps as soon as it is added in the view
    // hierarchy.
    self.backgroundColor = [UIColor clearColor];
    self.indicator.alpha = 0.;
    [UIView animateWithDuration:.3
                     animations:^{
                       self.backgroundColor =
                           [UIColor colorNamed:kScrimBackgroundColor];
                       self.indicator.alpha = 1.;
                     }
                     completion:NULL];
  }
  [super didMoveToSuperview];
}

@end
