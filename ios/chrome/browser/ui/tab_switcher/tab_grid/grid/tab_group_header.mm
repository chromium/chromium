// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_group_header.h"

namespace {
constexpr CGFloat kDotTitleSeparationMargin = 8;
constexpr CGFloat kColoredDotSize = 20;
}  // namespace

@implementation TabGroupHeader {
  // Title label.
  UILabel* _titleView;
  // Dot view.
  UIView* _coloredDotView;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _titleView = [self titleView];
    _coloredDotView = [self coloredDotView];

    [self addSubview:_coloredDotView];
    [self addSubview:_titleView];

    [NSLayoutConstraint activateConstraints:@[
      [_titleView.leadingAnchor
          constraintEqualToAnchor:_coloredDotView.trailingAnchor
                         constant:kDotTitleSeparationMargin],
      [_coloredDotView.centerYAnchor
          constraintEqualToAnchor:_titleView.centerYAnchor],
      [_coloredDotView.leadingAnchor
          constraintEqualToAnchor:self.leadingAnchor],
      [_titleView.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
      [_titleView.topAnchor constraintEqualToAnchor:self.topAnchor],
      [_titleView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
    ]];
  }
  return self;
}

- (void)setTitle:(NSString*)title {
  if ([_title isEqual:title]) {
    return;
  }
  _title = title;
  UIFontDescriptor* boldDescriptor = [[UIFontDescriptor
      preferredFontDescriptorWithTextStyle:UIFontTextStyleLargeTitle]
      fontDescriptorWithSymbolicTraits:UIFontDescriptorTraitBold];
  NSMutableAttributedString* boldTitle =
      [[NSMutableAttributedString alloc] initWithString:title];

  [boldTitle addAttribute:NSFontAttributeName
                    value:[UIFont fontWithDescriptor:boldDescriptor size:0.0]
                    range:NSMakeRange(0, title.length)];
  _titleView.attributedText = boldTitle;
}

- (void)setColor:(UIColor*)color {
  if ([_color isEqual:color]) {
    return;
  }
  _color = color;
  _coloredDotView.backgroundColor = color;
}

#pragma mark - Private

// Returns the group color dot view.
- (UIView*)coloredDotView {
  UIView* dotView = [[UIView alloc] initWithFrame:CGRectZero];
  dotView.translatesAutoresizingMaskIntoConstraints = NO;
  dotView.layer.cornerRadius = kColoredDotSize / 2;

  [NSLayoutConstraint activateConstraints:@[
    [dotView.heightAnchor constraintEqualToConstant:kColoredDotSize],
    [dotView.widthAnchor constraintEqualToConstant:kColoredDotSize],
  ]];

  return dotView;
}

// Returns the title label view.
- (UILabel*)titleView {
  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.textColor = UIColor.whiteColor;
  titleLabel.numberOfLines = 1;
  titleLabel.adjustsFontForContentSizeCategory = YES;
  titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  return titleLabel;
}

@end
