// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/bar_button_activity_indicator.h"

#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"

@implementation BarButtonActivityIndicator {
  UIActivityIndicatorView* _activityIndicator;
}

- (id)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _activityIndicator = GetMediumUIActivityIndicatorView();
    [_activityIndicator setBackgroundColor:[UIColor clearColor]];
    [_activityIndicator setHidesWhenStopped:YES];
    [_activityIndicator startAnimating];
    [self addSubview:_activityIndicator];
  }
  return self;
}

- (void)dealloc {
  [_activityIndicator stopAnimating];
}

- (void)layoutSubviews {
  [super layoutSubviews];
  CGSize boundsSize = self.bounds.size;
  CGPoint center = CGPointMake(boundsSize.width / 2, boundsSize.height / 2);
  [_activityIndicator setCenter:AlignPointToPixel(center)];
}

@end
