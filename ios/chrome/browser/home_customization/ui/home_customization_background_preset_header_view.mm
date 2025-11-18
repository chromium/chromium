// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/home_customization_background_preset_header_view.h"

#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {
// The minimum width the text label can have.
constexpr CGFloat kLabelMinWidth = 105.0;
}  // namespace

@implementation HomeCustomizationBackgroundPresetHeaderView {
  // Text label.
  UILabel* _textLabel;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.layer.masksToBounds = YES;
    self.translatesAutoresizingMaskIntoConstraints = NO;
    self.isAccessibilityElement = YES;
    self.accessibilityTraits = UIAccessibilityTraitHeader;
    _textLabel = [self createTextLabel];

    [self addSubview:_textLabel];

    [NSLayoutConstraint activateConstraints:@[
      [self.topAnchor constraintEqualToAnchor:_textLabel.topAnchor],
      [self.bottomAnchor constraintEqualToAnchor:_textLabel.bottomAnchor],
      [_textLabel.widthAnchor
          constraintGreaterThanOrEqualToConstant:kLabelMinWidth]
    ]];
  }
  return self;
}

- (void)layoutSubviews {
  [super layoutSubviews];
  _textLabel.layer.cornerRadius = _textLabel.frame.size.height / 2.0;
  _textLabel.layer.masksToBounds = YES;
}

#pragma mark - Setters

- (void)setText:(NSString*)text {
  _textLabel.backgroundColor = [UIColor clearColor];
  _textLabel.text = text;
  self.accessibilityLabel = text;
}

#pragma mark - Private

// Returns the text label view.
- (UILabel*)createTextLabel {
  UILabel* textLabel = [[UILabel alloc] init];
  // When the text is empty, we set it to a single space to avoid hiding the
  // label. This ensures the width constraint is respected and the label remains
  // visible.
  textLabel.text = @" ";
  textLabel.backgroundColor = [UIColor colorNamed:kGrey100Color];
  textLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  textLabel.textAlignment = NSTextAlignmentLeft;
  textLabel.font =
      PreferredFontForTextStyle(UIFontTextStyleHeadline, UIFontWeightSemibold);
  textLabel.adjustsFontForContentSizeCategory = YES;
  textLabel.translatesAutoresizingMaskIntoConstraints = NO;
  return textLabel;
}

@end
