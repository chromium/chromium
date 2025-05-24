// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/tab_group_header.h"

namespace {
constexpr CGFloat kDotTitleSeparationMargin = 8;
constexpr CGFloat kColoredDotSize = 20;
}  // namespace

@implementation TabGroupHeader {
  // Title label.
  UILabel* _titleView;
  // Dot view.
  UIView* _coloredDotView;
  // Container for the whole title.
  UIView* _container;
  // Constraints for regular width.
  NSArray<NSLayoutConstraint*>* _regularWidthConstraints;
  // Constraints for compact width.
  NSArray<NSLayoutConstraint*>* _compactWidthConstraints;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _titleView = [self titleView];
    _coloredDotView = [self coloredDotView];
    _container = [[UIView alloc] init];
    _container.translatesAutoresizingMaskIntoConstraints = NO;

    [self addSubview:_container];
    [_container addSubview:_coloredDotView];
    [_container addSubview:_titleView];

    _regularWidthConstraints = @[
      [_container.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],
      [_container.widthAnchor
          constraintLessThanOrEqualToAnchor:self.widthAnchor],
    ];

    _compactWidthConstraints = @[
      [_container.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
      [_container.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
    ];

    [NSLayoutConstraint activateConstraints:@[
      [_coloredDotView.leadingAnchor
          constraintEqualToAnchor:_container.leadingAnchor],
      [_coloredDotView.centerYAnchor
          constraintEqualToAnchor:_titleView.centerYAnchor],

      [_titleView.leadingAnchor
          constraintEqualToAnchor:_coloredDotView.trailingAnchor
                         constant:kDotTitleSeparationMargin],

      [_titleView.trailingAnchor
          constraintEqualToAnchor:_container.trailingAnchor],
      [_titleView.topAnchor constraintEqualToAnchor:_container.topAnchor],
      [_titleView.bottomAnchor constraintEqualToAnchor:_container.bottomAnchor],

      [_container.topAnchor constraintEqualToAnchor:self.topAnchor],
      [_container.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
    ]];

    if (self.traitCollection.horizontalSizeClass ==
        UIUserInterfaceSizeClassRegular) {
      [NSLayoutConstraint activateConstraints:_regularWidthConstraints];
    } else {
      [NSLayoutConstraint activateConstraints:_compactWidthConstraints];
    }

    if (@available(iOS 17, *)) {
      [self registerForTraitChanges:@[ UITraitHorizontalSizeClass.class ]
                         withAction:@selector(horizontalSizeClassDidChange)];
    }
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

// Called when the horizontal size class have changed.
- (void)horizontalSizeClassDidChange {
  if (self.traitCollection.horizontalSizeClass ==
      UIUserInterfaceSizeClassRegular) {
    [NSLayoutConstraint deactivateConstraints:_compactWidthConstraints];
    [NSLayoutConstraint activateConstraints:_regularWidthConstraints];
  } else {
    [NSLayoutConstraint deactivateConstraints:_regularWidthConstraints];
    [NSLayoutConstraint activateConstraints:_compactWidthConstraints];
  }
}

@end
