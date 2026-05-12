// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_header_view.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {
// Vertical constant for the header label.
const CGFloat kHeaderLabelVerticalPadding = 10.0f;
}  // namespace

@implementation ComposeboxMenuHeaderView

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.accessibilityTraits |= UIAccessibilityTraitHeader;
    _label = [[UILabel alloc] init];
    _label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    _label.adjustsFontForContentSizeCategory = YES;
    _label.textColor = [UIColor colorNamed:kTextPrimaryColor];
    _label.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_label];

    [NSLayoutConstraint activateConstraints:@[
      [_label.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
      [_label.topAnchor constraintEqualToAnchor:self.topAnchor
                                       constant:kHeaderLabelVerticalPadding],
      [_label.bottomAnchor
          constraintEqualToAnchor:self.bottomAnchor
                         constant:-kHeaderLabelVerticalPadding],
      [_label.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
    ]];
  }
  return self;
}

@end
