// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/cells/payments_selector_edit_item.h"

#include <algorithm>

#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Padding used on the leading and trailing edges of the cell and between the
// two labels.
const CGFloat kHorizontalPadding = 16;

// Padding used on the top and bottom edges of the cell.
const CGFloat kVerticalPadding = 16;

// Minimum proportion of the available width to guarantee to the main and detail
// labels.
const CGFloat kMinTextWidthRatio = 0.75f;
const CGFloat kMinDetailTextWidthRatio = 0.25f;
}

@implementation PaymentsSelectorEditItem

@synthesize accessoryType = _accessoryType;
@synthesize complete = _complete;
@synthesize name = _name;
@synthesize value = _value;
@synthesize autofillUIType = _autofillUIType;
@synthesize required = _required;
@synthesize nameFont = _nameFont;
@synthesize nameColor = _nameColor;
@synthesize valueFont = _valueFont;
@synthesize valueColor = _valueColor;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [CollectionViewDetailCell class];
  }
  return self;
}

- (UIFont*)nameFont {
  if (!_nameFont) {
    _nameFont = [[UIFontMetrics defaultMetrics]
        scaledFontForFont:[MDCTypography body2Font]];
  }
  return _nameFont;
}

- (UIColor*)nameColor {
  if (!_nameColor) {
    _nameColor = [UIColor colorNamed:kTextPrimaryColor];
  }
  return _nameColor;
}

- (UIFont*)valueFont {
  if (!_valueFont) {
    _valueFont = [[UIFontMetrics defaultMetrics]
        scaledFontForFont:[MDCTypography body1Font]];
  }
  return _valueFont;
}

- (UIColor*)valueColor {
  if (!_valueColor) {
    _valueColor = [UIColor colorNamed:kTextSecondaryColor];
  }
  return _valueColor;
}

#pragma mark CollectionViewItem

- (void)configureCell:(CollectionViewDetailCell*)cell {
  [super configureCell:cell];
  [cell cr_setAccessoryType:self.accessoryType];
  NSString* textLabelFormat = self.required ? @"%@*" : @"%@";
  cell.textLabel.text = [NSString stringWithFormat:textLabelFormat, self.name];
  cell.detailTextLabel.text = self.value;

  // Styling.
  cell.textLabel.font = self.nameFont;
  cell.textLabel.textColor = self.nameColor;
  cell.detailTextLabel.font = self.valueFont;
  cell.detailTextLabel.textColor = self.valueColor;
  cell.textLabel.adjustsFontForContentSizeCategory = YES;
  cell.detailTextLabel.adjustsFontForContentSizeCategory = YES;

  [cell updateConstraintsIfNeeded];
}

@end

@implementation CollectionViewDetailCell {
  NSLayoutConstraint* _textLabelWidthConstraint;
  NSLayoutConstraint* _detailTextLabelWidthConstraint;
}

@synthesize textLabel = _textLabel;
@synthesize detailTextLabel = _detailTextLabel;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.isAccessibilityElement = YES;
    UIView* contentView = self.contentView;

    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _textLabel.backgroundColor = [UIColor clearColor];
    [contentView addSubview:_textLabel];

    _textLabel.font = [[MDCTypography fontLoader] mediumFontOfSize:14];
    _textLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];

    _detailTextLabel = [[UILabel alloc] init];
    _detailTextLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _detailTextLabel.backgroundColor = [UIColor clearColor];
    [contentView addSubview:_detailTextLabel];

    _detailTextLabel.font = [[MDCTypography fontLoader] regularFontOfSize:14];
    _detailTextLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];

    // Set up the width constraints. They are activated here and updated in
    // layoutSubviews.
    _textLabelWidthConstraint =
        [_textLabel.widthAnchor constraintEqualToConstant:0];
    _detailTextLabelWidthConstraint =
        [_detailTextLabel.widthAnchor constraintEqualToConstant:0];

    [NSLayoutConstraint activateConstraints:@[
      // Fix the leading and trailing edges of the text labels.
      [_textLabel.leadingAnchor
          constraintEqualToAnchor:contentView.leadingAnchor
                         constant:kHorizontalPadding],
      [_detailTextLabel.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor
                         constant:-kHorizontalPadding],

      // Set up the top and bottom constraints for |_textLabel| and align the
      // baselines of the two text labels.
      [_textLabel.topAnchor constraintEqualToAnchor:contentView.topAnchor
                                           constant:kVerticalPadding],
      [_textLabel.bottomAnchor constraintEqualToAnchor:contentView.bottomAnchor
                                              constant:-kVerticalPadding],
      [_detailTextLabel.firstBaselineAnchor
          constraintEqualToAnchor:_textLabel.firstBaselineAnchor],

      _textLabelWidthConstraint,
      _detailTextLabelWidthConstraint,
    ]];
  }
  return self;
}

// Updates the layout constraints of the text labels and then calls the
// superclass's implementation of layoutSubviews which can then take account of
// the new constraints.
- (void)layoutSubviews {
  [super layoutSubviews];

  // Size the labels in order to determine how much width they want.
  [self.textLabel sizeToFit];
  [self.detailTextLabel sizeToFit];

  // Update the width constraints.
  _textLabelWidthConstraint.constant = self.textLabelTargetWidth;
  _detailTextLabelWidthConstraint.constant = self.detailTextLabelTargetWidth;

  // Now invoke the layout.
  [super layoutSubviews];
}

- (CGFloat)textLabelTargetWidth {
  CGFloat availableWidth =
      self.contentView.bounds.size.width - (3 * kHorizontalPadding);
  CGFloat textLabelWidth = self.textLabel.frame.size.width;
  CGFloat detailTextLabelWidth = self.detailTextLabel.frame.size.width;

  if (textLabelWidth + detailTextLabelWidth <= availableWidth)
    return textLabelWidth;

  return std::max(
      availableWidth - detailTextLabelWidth,
      std::min(availableWidth * kMinTextWidthRatio, textLabelWidth));
}

- (CGFloat)detailTextLabelTargetWidth {
  CGFloat availableWidth =
      self.contentView.bounds.size.width - (3 * kHorizontalPadding);
  CGFloat textLabelWidth = self.textLabel.frame.size.width;
  CGFloat detailTextLabelWidth = self.detailTextLabel.frame.size.width;

  if (textLabelWidth + detailTextLabelWidth <= availableWidth)
    return detailTextLabelWidth;

  return std::max(availableWidth - textLabelWidth,
                  std::min(availableWidth * kMinDetailTextWidthRatio,
                           detailTextLabelWidth));
}

- (NSString*)accessibilityLabel {
  return self.textLabel.text;
}

- (NSString*)accessibilityValue {
  return self.detailTextLabel.text;
}

@end
