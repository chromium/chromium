// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/home_customization_background_cell.h"

namespace {

// Corner radius of the background cell.
const CGFloat kCornerRadius = 8.1;

}  // namespace

@implementation HomeCustomizationBackgroundCell

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.backgroundColor = [UIColor colorNamed:@"ntp_background_color"];
    self.layer.cornerRadius = kCornerRadius;
    self.layer.masksToBounds = YES;
  }
  return self;
}

@end
