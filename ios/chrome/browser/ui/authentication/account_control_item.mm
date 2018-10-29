// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/account_control_item.h"

#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#include "ios/chrome/browser/ui/collection_view/cells/collection_view_cell_constants.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Padding used on the leading and trailing edges of the cell.
const CGFloat kHorizontalPadding = 16;

// Padding used on the top and bottom edges of the cell.
const CGFloat kVerticalPadding = 16;

// Padding used between the image and text.
const CGFloat kHorizontalPaddingBetweenImageAndText = 16;

// Padding between top label and detail label.
const CGFloat kVerticalPaddingBetweenLabelAndDetailLabel = 8;
}  // namespace

@implementation AccountControlItem

@synthesize cellStyle = _cellStyle;
@synthesize image = _image;
@synthesize text = _text;
@synthesize detailText = _detailText;
@synthesize accessoryType = _accessoryType;
@synthesize shouldDisplayError = _shouldDisplayError;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [AccountControlCell class];
    self.accessibilityTraits |= UIAccessibilityTraitButton;
  }
  return self;
}

#pragma mark - CollectionViewItem

- (void)configureCell:(AccountControlCell*)cell {
  [super configureCell:cell];
  cell.imageView.image = self.image;
  [cell cr_setAccessoryType:self.accessoryType];

  BOOL uikitStyle = self.cellStyle == CollectionViewCellStyle::kUIKit;
  UIFont* textFont = uikitStyle ? [UIFont systemFontOfSize:kUIKitMainFontSize]
                                : [MDCTypography body2Font];
  UIColor* textColor = uikitStyle ? UIColorFromRGB(kUIKitMainTextColor)
                                  : [[MDCPalette greyPalette] tint900];
  UIFont* detailTextFont =
      uikitStyle ? [UIFont systemFontOfSize:kUIKitMultilineDetailFontSize]
                 : [MDCTypography body1Font];

  UIColor* detailTextColor =
      uikitStyle ? UIColorFromRGB(kUIKitMultilineDetailTextColor)
                 : [[MDCPalette greyPalette] tint700];
  if (self.shouldDisplayError) {
    detailTextColor = [[MDCPalette cr_redPalette] tint700];
  }

  cell.textLabel.attributedText =
      [self attributedStringForText:self.text font:textFont color:textColor];
  cell.detailTextLabel.attributedText =
      [self attributedStringForText:self.detailText
                               font:detailTextFont
                              color:detailTextColor];
}

#pragma mark - Helper methods

- (NSAttributedString*)attributedStringForText:(NSString*)text
                                          font:(UIFont*)font
                                         color:(UIColor*)color {
  NSMutableParagraphStyle* paragraphStyle =
      [[NSMutableParagraphStyle alloc] init];
  paragraphStyle.lineHeightMultiple = 1.15;
  return [[NSAttributedString alloc]
      initWithString:text
          attributes:@{
            NSParagraphStyleAttributeName : paragraphStyle,
            NSFontAttributeName : font,
            NSForegroundColorAttributeName : color
          }];
}

@end

@interface AccountControlCell () {
  // Constraint used to set padding between image and text when image exists.
  NSLayoutConstraint* _textLeadingAnchorConstraint;
}
@end

@implementation AccountControlCell

@synthesize imageView = _imageView;
@synthesize textLabel = _textLabel;
@synthesize detailTextLabel = _detailTextLabel;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.isAccessibilityElement = YES;
    [self addSubviews];
    [self setDefaultViewStyling];
    [self setViewConstraints];
  }
  return self;
}

// Create and add subviews.
- (void)addSubviews {
  UIView* contentView = self.contentView;
  contentView.clipsToBounds = YES;

  _imageView = [[UIImageView alloc] init];
  _imageView.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:_imageView];

  _textLabel = [[UILabel alloc] init];
  _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:_textLabel];

  _detailTextLabel = [[UILabel alloc] init];
  _detailTextLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:_detailTextLabel];
}

// Set default imageView styling and default font and text colors for labels.
- (void)setDefaultViewStyling {
  _imageView.contentMode = UIViewContentModeCenter;

  _textLabel.font = [MDCTypography body2Font];
  _textLabel.textColor = [[MDCPalette greyPalette] tint900];
  _detailTextLabel.font = [MDCTypography body1Font];
  _detailTextLabel.numberOfLines = 0;
}

// Set constraints on subviews.
- (void)setViewConstraints {
  UIView* contentView = self.contentView;

  _textLeadingAnchorConstraint = [_textLabel.leadingAnchor
      constraintEqualToAnchor:_imageView.trailingAnchor];

  [NSLayoutConstraint activateConstraints:@[
    // Set leading anchors.
    [_imageView.leadingAnchor constraintEqualToAnchor:contentView.leadingAnchor
                                             constant:kHorizontalPadding],
    [_detailTextLabel.leadingAnchor
        constraintEqualToAnchor:_textLabel.leadingAnchor],
    _textLeadingAnchorConstraint,

    // Set vertical anchors.
    [_textLabel.topAnchor constraintEqualToAnchor:contentView.topAnchor
                                         constant:kVerticalPadding],
    [_textLabel.bottomAnchor
        constraintEqualToAnchor:_detailTextLabel.topAnchor
                       constant:-kVerticalPaddingBetweenLabelAndDetailLabel],
    [_imageView.centerYAnchor constraintEqualToAnchor:_textLabel.centerYAnchor],
    [_detailTextLabel.bottomAnchor
        constraintEqualToAnchor:contentView.bottomAnchor
                       constant:-kVerticalPadding],

    // Set trailing anchors.
    [_textLabel.trailingAnchor
        constraintLessThanOrEqualToAnchor:contentView.trailingAnchor
                                 constant:-kHorizontalPadding],
    [_detailTextLabel.trailingAnchor
        constraintLessThanOrEqualToAnchor:contentView.trailingAnchor
                                 constant:-kHorizontalPadding],
  ]];
}

#pragma mark - UIView

- (void)layoutSubviews {
  [super layoutSubviews];

  // Adjust the text label preferredMaxLayoutWidth when the parent's width
  // changes, for instance on screen rotation.
  CGFloat parentWidth = self.contentView.frame.size.width;
  if (_imageView.image) {
    _detailTextLabel.preferredMaxLayoutWidth =
        parentWidth - 2.f * kHorizontalPadding -
        kHorizontalPaddingBetweenImageAndText - _imageView.image.size.width;
    _textLeadingAnchorConstraint.constant =
        kHorizontalPaddingBetweenImageAndText;
  } else {
    _detailTextLabel.preferredMaxLayoutWidth =
        parentWidth - 2.f * kHorizontalPadding;
    _textLeadingAnchorConstraint.constant = 0;
  }

  // Re-layout with the new preferred width to allow the label to adjust its
  // height.
  [super layoutSubviews];
}

#pragma mark - UICollectionReusableView

- (void)prepareForReuse {
  [super prepareForReuse];
  self.imageView.image = nil;
  self.textLabel.text = nil;
  self.detailTextLabel.text = nil;
  self.accessoryType = MDCCollectionViewCellAccessoryNone;
  self.detailTextLabel.textColor = [[MDCPalette greyPalette] tint700];
}

#pragma mark - NSObject(Accessibility)

- (NSString*)accessibilityLabel {
  return [NSString stringWithFormat:@"%@, %@", self.textLabel.text,
                                    self.detailTextLabel.text];
}

@end
