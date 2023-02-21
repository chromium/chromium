// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_button_header.h"

#import <QuartzCore/QuartzCore.h>

#import "ios/chrome/browser/tabs/inactive_tabs/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_button.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The margin at the top of the header.
const CGFloat kTopMargin = 12;
// The margin on the other edges of the header.
const CGFloat kMargin = 16;
}  // namespace

@implementation InactiveTabsButtonHeader

- (instancetype)initWithFrame:(CGRect)frame {
  DCHECK(IsInactiveTabsEnabled());
  self = [super initWithFrame:frame];
  if (self) {
    _button = [[InactiveTabsButton alloc] init];
    _button.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_button];

    [NSLayoutConstraint activateConstraints:@[
      [_button.topAnchor constraintEqualToAnchor:self.topAnchor
                                        constant:kTopMargin],
      [_button.leadingAnchor constraintEqualToAnchor:self.leadingAnchor
                                            constant:kMargin],
      [_button.bottomAnchor constraintEqualToAnchor:self.bottomAnchor
                                           constant:-kMargin],
      [_button.trailingAnchor constraintEqualToAnchor:self.trailingAnchor
                                             constant:-kMargin],
    ]];
  }
  return self;
}

@end
