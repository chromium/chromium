// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/text_and_error_item.h"

#include "base/mac/foundation_util.h"
#include "ios/chrome/browser/ui/collection_view/cells/collection_view_cell_constants.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
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
const CGFloat kHorizontalPaddingBetweenImageAndText = 10;

// Error icon fixed horizontal size.
const CGFloat kHorizontalErrorIconFixedSize = 25;
}  // namespace

@implementation TextAndErrorItem

@synthesize text = _text;
@synthesize accessoryType = _accessoryType;
@synthesize shouldDisplayError = _shouldDisplayError;
@synthesize enabled = _enabled;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [TextAndErrorCell class];
    self.accessibilityTraits |= UIAccessibilityTraitButton;
    self.enabled = YES;
  }
  return self;
}

- (void)configureCell:(TextAndErrorCell*)cell {
  [super configureCell:cell];
  cell.textLabel.text = self.text;
  cell.accessoryType = self.accessoryType;
  cell.accessibilityLabel = self.text;
  if (self.shouldDisplayError) {
    cell.errorIcon.image = [UIImage imageNamed:@"settings_error"];
  } else {
    cell.errorIcon.image = nil;
  }
  UIImageView* accessoryImage =
      base::mac::ObjCCastStrict<UIImageView>(cell.accessoryView);
  accessoryImage.image = [accessoryImage.image
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  if (self.isEnabled) {
    cell.textLabel.textColor = [[MDCPalette greyPalette] tint900];
    [cell setUserInteractionEnabled:YES];
    [accessoryImage setTintColor:[[MDCPalette greyPalette] tint400]];
  } else {
    [accessoryImage setTintColor:[[[MDCPalette greyPalette] tint200]
                                     colorWithAlphaComponent:0.5]];
    cell.textLabel.textColor = [[MDCPalette greyPalette] tint500];
    [cell setUserInteractionEnabled:NO];
  }
}

@end

@interface TextAndErrorCell () {
  // Constraint used to set the errorIcon width depending on its existence.
  NSLayoutConstraint* _errorIconWidthConstraint;
}

@end

@implementation TextAndErrorCell

@synthesize textLabel = _textLabel;
@synthesize errorIcon = _errorIcon;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.contentView.clipsToBounds = YES;
    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _textLabel.font = [UIFont systemFontOfSize:kUIKitMainFontSize];
    _textLabel.textColor = UIColorFromRGB(kUIKitMainTextColor);
    _textLabel.numberOfLines = 0;
    [self.contentView addSubview:_textLabel];
    _errorIcon = [[UIImageView alloc] init];
    _errorIcon.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:_errorIcon];

    [self setConstraints];
  }
  return self;
}

- (void)setConstraints {
  UIView* contentView = self.contentView;

  _errorIconWidthConstraint = [_errorIcon.widthAnchor
      constraintEqualToConstant:kHorizontalErrorIconFixedSize];

  [NSLayoutConstraint activateConstraints:@[
    [_textLabel.leadingAnchor constraintEqualToAnchor:contentView.leadingAnchor
                                             constant:kHorizontalPadding],
    [_textLabel.trailingAnchor
        constraintEqualToAnchor:_errorIcon.leadingAnchor
                       constant:-kHorizontalPaddingBetweenImageAndText],
    [_errorIcon.trailingAnchor
        constraintEqualToAnchor:contentView.trailingAnchor
                       constant:-kHorizontalPadding],
    [_errorIcon.centerYAnchor
        constraintEqualToAnchor:contentView.centerYAnchor],
    _errorIconWidthConstraint,
  ]];

  AddOptionalVerticalPadding(contentView, _textLabel, kVerticalPadding);
}

// Implement -layoutSubviews as per instructions in documentation for
// +[MDCCollectionViewCell cr_preferredHeightForWidth:forItem:].
- (void)layoutSubviews {
  [super layoutSubviews];

  // Adjust the text and detailText label preferredMaxLayoutWidth when the
  // parent's width
  // changes, for instance on screen rotation.
  if (_errorIcon.image) {
    _errorIconWidthConstraint.constant = kHorizontalErrorIconFixedSize;
    _textLabel.preferredMaxLayoutWidth =
        CGRectGetWidth(self.contentView.frame) -
        CGRectGetWidth(_errorIcon.frame) - 2 * kHorizontalPadding -
        kHorizontalPaddingBetweenImageAndText;
  } else {
    _errorIconWidthConstraint.constant = 0;
    _textLabel.preferredMaxLayoutWidth =
        CGRectGetWidth(self.contentView.frame) - 2 * kHorizontalPadding;
  }

  // Re-layout with the new preferred width to allow the label to adjust its
  // height.
  [super layoutSubviews];
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.textLabel.textColor = [[MDCPalette greyPalette] tint900];
  [self setUserInteractionEnabled:YES];
  UIImageView* accessoryImage =
      base::mac::ObjCCastStrict<UIImageView>(self.accessoryView);
  accessoryImage.image = [accessoryImage.image
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  [accessoryImage setTintColor:[[MDCPalette greyPalette] tint400]];
}

@end
