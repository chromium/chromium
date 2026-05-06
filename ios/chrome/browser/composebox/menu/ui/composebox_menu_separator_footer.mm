// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_separator_footer.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {
// Height of the separator line.
const CGFloat kSeparatorHeight = 1.0f;
}  // namespace

@implementation ComposeboxMenuSeparatorFooter

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    UIView* separator = [[UIView alloc] init];
    separator.backgroundColor = [UIColor colorNamed:kSeparatorColor];
    separator.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:separator];

    [NSLayoutConstraint activateConstraints:@[
      [separator.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
      [separator.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
      [separator.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
      [separator.heightAnchor constraintEqualToConstant:kSeparatorHeight],
    ]];
  }
  return self;
}

@end
