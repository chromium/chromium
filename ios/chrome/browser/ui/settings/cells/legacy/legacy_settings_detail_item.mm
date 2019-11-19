// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/legacy/legacy_settings_detail_item.h"

#include <algorithm>

#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#include "ios/chrome/browser/ui/collection_view/cells/collection_view_cell_constants.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Padding used on the leading and trailing edges of the cell and between the
// two labels.
const CGFloat kHorizontalPadding = 16;

// Padding used between the icon and the text labels.
const CGFloat kIconTrailingPadding = 12;

// Padding used on the top and bottom edges of the cell.
const CGFloat kVerticalPadding = 16;

// Size of the icon image.
const CGFloat kIconImageSize = 28;

// Minimum proportion of the available width to guarantee to the main and detail
// labels.
const CGFloat kMinTextWidthRatio = 0.75f;
const CGFloat kMinDetailTextWidthRatio = 0.25f;
}  // namespace

@implementation LegacySettingsDetailItem

@synthesize accessoryType = _accessoryType;
@synthesize iconImageName = _iconImageName;
@synthesize text = _text;
@synthesize detailText = _detailText;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [LegacySettingsDetailCell class];
  }
  return self;
}

#pragma mark CollectionViewItem

- (void)configureCell:(LegacySettingsDetailCell*)cell {
  [super configureCell:cell];
  [cell cr_setAccessoryType:self.accessoryType];
  cell.textLabel.text = self.text;
  cell.detailTextLabel.text = self.detailText;

  // Update the icon image, if one is present.
  UIImage* iconImage = nil;
  if ([self.iconImageName length]) {
    iconImage = [UIImage imageNamed:self.iconImageName];
  }
  [cell setIconImage:iconImage];
}

@end

@implementation LegacySettingsDetailCell {
  UIImageView* _iconImageView;
  UILayoutGuide* _labelContainerGuide;
  NSLayoutConstraint* _iconHiddenConstraint;
  NSLayoutConstraint* _iconVisibleConstraint;
  NSLayoutConstraint* _textLabelWidthConstraint;
  NSLayoutConstraint* _detailTextLabelWidthConstraint;
}

@synthesize detailTextLabel = _detailTextLabel;
@synthesize textLabel = _textLabel;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.isAccessibilityElement = YES;
    UIView* contentView = self.contentView;

    _iconImageView = [[UIImageView alloc] init];
    _iconImageView.translatesAutoresizingMaskIntoConstraints = NO;
    _iconImageView.hidden = YES;
    [contentView addSubview:_iconImageView];

    // Constrain the labels inside a container view, to make width computations
    // easier.
    _labelContainerGuide = [[UILayoutGuide alloc] init];
    [contentView addLayoutGuide:_labelContainerGuide];

    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _textLabel.font = [UIFont systemFontOfSize:kUIKitMainFontSize];
    _textLabel.textColor = UIColor.cr_labelColor;
    _textLabel.backgroundColor = UIColor.clearColor;
    [contentView addSubview:_textLabel];

    _detailTextLabel = [[UILabel alloc] init];
    _detailTextLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _detailTextLabel.font = [UIFont systemFontOfSize:kUIKitDetailFontSize];
    _detailTextLabel.textColor = UIColor.cr_secondaryLabelColor;
    _detailTextLabel.backgroundColor = UIColor.clearColor;
    [contentView addSubview:_detailTextLabel];

    // Set up the width constraints. They are activated here and updated in
    // layoutSubviews.
    _textLabelWidthConstraint =
        [_textLabel.widthAnchor constraintEqualToConstant:0];
    _detailTextLabelWidthConstraint =
        [_detailTextLabel.widthAnchor constraintEqualToConstant:0];

    // Set up the constraints for when the icon is visible and hidden.  One of
    // these will be active at a time, defaulting to hidden.
    _iconHiddenConstraint = [_labelContainerGuide.leadingAnchor
        constraintEqualToAnchor:contentView.leadingAnchor
                       constant:kHorizontalPadding];
    _iconVisibleConstraint = [_labelContainerGuide.leadingAnchor
        constraintEqualToAnchor:_iconImageView.trailingAnchor
                       constant:kIconTrailingPadding];

    [NSLayoutConstraint activateConstraints:@[
      [_iconImageView.leadingAnchor
          constraintEqualToAnchor:contentView.leadingAnchor
                         constant:kHorizontalPadding],
      [_iconImageView.widthAnchor constraintEqualToConstant:kIconImageSize],
      [_iconImageView.heightAnchor constraintEqualToConstant:kIconImageSize],

      // Fix the edges of the text labels.
      [_textLabel.leadingAnchor
          constraintEqualToAnchor:_labelContainerGuide.leadingAnchor],
      [_detailTextLabel.trailingAnchor
          constraintEqualToAnchor:_labelContainerGuide.trailingAnchor],
      [_labelContainerGuide.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor
                         constant:-kHorizontalPadding],

      // Set up the vertical constraints and align the baselines of the two text
      // labels.
      [_iconImageView.centerYAnchor
          constraintEqualToAnchor:contentView.centerYAnchor],
      [_textLabel.centerYAnchor
          constraintEqualToAnchor:contentView.centerYAnchor],
      [_detailTextLabel.firstBaselineAnchor
          constraintEqualToAnchor:_textLabel.firstBaselineAnchor],

      _textLabelWidthConstraint,
      _detailTextLabelWidthConstraint,
      _iconHiddenConstraint,
    ]];

    AddOptionalVerticalPadding(contentView, _textLabel, kVerticalPadding);
  }
  return self;
}

- (void)setIconImage:(UIImage*)image {
  BOOL hidden = (image == nil);
  if (hidden == _iconImageView.hidden) {
    return;
  }

  _iconImageView.image = image;
  _iconImageView.hidden = hidden;
  if (hidden) {
    _iconVisibleConstraint.active = NO;
    _iconHiddenConstraint.active = YES;
  } else {
    _iconHiddenConstraint.active = NO;
    _iconVisibleConstraint.active = YES;
  }
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

- (void)prepareForReuse {
  [super prepareForReuse];

  [self setIconImage:nil];
}

- (CGFloat)textLabelTargetWidth {
  CGFloat availableWidth =
      CGRectGetWidth(_labelContainerGuide.layoutFrame) - kHorizontalPadding;
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
      CGRectGetWidth(_labelContainerGuide.layoutFrame) - kHorizontalPadding;
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
