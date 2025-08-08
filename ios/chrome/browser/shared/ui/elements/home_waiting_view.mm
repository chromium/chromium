// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/elements/home_waiting_view.h"

#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

@implementation HomeWaitingView {
  UIActivityIndicatorView* _activityIndicator;
}

- (instancetype)initWithFrame:(CGRect)frame backgroundColor:(UIColor*)color {
  self = [super initWithFrame:frame];
  if (self) {
    self.backgroundColor = color;
    self.autoresizingMask =
        UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  }
  return self;
}

- (void)startWaiting {
  dispatch_time_t delayForIndicatorAppearance =
      dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.5 * NSEC_PER_SEC));
  __weak __typeof(self) weakSelf = self;
  dispatch_after(delayForIndicatorAppearance, dispatch_get_main_queue(), ^{
    [weakSelf startActivityIndiactor];
  });
}

- (void)stopWaitingWithCompletion:(ProceduralBlock)completion {
  [_activityIndicator stopAnimating];
  if (completion) {
    completion();
  }
}

#pragma mark - Private

// Configures and starts the activity indicator.
- (void)startActivityIndiactor {
  _activityIndicator = [[UIActivityIndicatorView alloc] init];
  _activityIndicator.color = [UIColor colorNamed:kBlueColor];
  _activityIndicator.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  [self addSubview:_activityIndicator];
  [_activityIndicator startAnimating];
}

@end
