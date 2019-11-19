// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_account_item.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#include "ios/chrome/browser/ui/collection_view/cells/collection_view_cell_constants.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Padding used on the leading and trailing edges of the cell.
const CGFloat kHorizontalPadding = 16;

// Padding used between the image and text.
const CGFloat kHorizontalPaddingBetweenImageAndText = 10;

// Padding used between the text and error icon.
const CGFloat kHorizontalPaddingBetweenTextAndError = 5;

// Image fixed horizontal size.
const CGFloat kHorizontalImageFixedSize = 40;

// Error icon fixed horizontal size.
const CGFloat kHorizontalErrorIconFixedSize = 25;
}

@interface CollectionViewAccountCell ()
// Updates the cell's fonts and colors for the given |cellStyle|.
- (void)updateForStyle:(CollectionViewCellStyle)cellStyle;
@end

@implementation CollectionViewAccountItem

@synthesize cellStyle = _cellStyle;
@synthesize image = _image;
@synthesize text = _text;
@synthesize detailText = _detailText;
@synthesize accessoryType = _accessoryType;
@synthesize shouldDisplayError = _shouldDisplayError;
@synthesize chromeIdentity = _chromeIdentity;
@synthesize enabled = _enabled;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [CollectionViewAccountCell class];
    self.accessibilityTraits |= UIAccessibilityTraitButton;
    _cellStyle = CollectionViewCellStyle::kMaterial;
    _enabled = YES;
  }
  return self;
}

#pragma mark - CollectionViewItem

- (void)configureCell:(CollectionViewAccountCell*)cell {
  [super configureCell:cell];

  [cell updateForStyle:self.cellStyle];
  cell.imageView.image = self.image;
  cell.textLabel.text = self.text;
  cell.detailTextLabel.text = self.detailText;
  [cell cr_setAccessoryType:self.accessoryType];
  if (self.shouldDisplayError) {
    cell.errorIcon.image = [[UIImage imageNamed:@"settings_error"]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    cell.errorIcon.tintColor = [UIColor colorNamed:kRedColor];
    cell.detailTextLabel.textColor = [UIColor colorNamed:kRedColor];
  } else {
    cell.errorIcon.image = nil;
    cell.detailTextLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  }

  if (self.isEnabled) {
    cell.userInteractionEnabled = YES;
    cell.contentView.alpha = 1;
    UIImageView* accessoryImage =
        base::mac::ObjCCastStrict<UIImageView>(cell.accessoryView);
    accessoryImage.tintColor =
        [accessoryImage.tintColor colorWithAlphaComponent:1];
  } else {
    cell.userInteractionEnabled = NO;
    cell.contentView.alpha = 0.5;
    UIImageView* accessoryImage =
        base::mac::ObjCCastStrict<UIImageView>(cell.accessoryView);
    accessoryImage.tintColor =
        [accessoryImage.tintColor colorWithAlphaComponent:0.5];
  }
}

@end

@interface CollectionViewAccountCell () {
  // Constraint used to set padding between image and text when image exists.
  NSLayoutConstraint* _textLeadingAnchorConstraint;

  // Constraint used to set the errorIcon width depending on it's existence.
  NSLayoutConstraint* _errorIconWidthConstraint;
}
@end

@implementation CollectionViewAccountCell

@synthesize imageView = _imageView;
@synthesize textLabel = _textLabel;
@synthesize detailTextLabel = _detailTextLabel;
@synthesize errorIcon = _errorIcon;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.isAccessibilityElement = YES;
    [self addSubviews];
    [self updateForStyle:CollectionViewCellStyle::kMaterial];
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

  _errorIcon = [[UIImageView alloc] init];
  _errorIcon.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:_errorIcon];

  _textLabel = [[UILabel alloc] init];
  _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:_textLabel];

  _detailTextLabel = [[UILabel alloc] init];
  _detailTextLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:_detailTextLabel];
}

- (void)updateForStyle:(CollectionViewCellStyle)cellStyle {
  _imageView.contentMode = UIViewContentModeCenter;
  _imageView.layer.masksToBounds = YES;
  _imageView.contentMode = UIViewContentModeScaleAspectFit;

  if (cellStyle == CollectionViewCellStyle::kUIKit) {
    _textLabel.font = [UIFont systemFontOfSize:kUIKitMainFontSize];
    _textLabel.textColor = UIColor.cr_labelColor;
    _detailTextLabel.font =
        [UIFont systemFontOfSize:kUIKitMultilineDetailFontSize];
    _detailTextLabel.textColor = UIColor.cr_secondaryLabelColor;
  } else {
    _textLabel.font = [[MDCTypography fontLoader] mediumFontOfSize:14];
    _textLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    _detailTextLabel.font = [[MDCTypography fontLoader] regularFontOfSize:14];
    _detailTextLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  }
}

// Set constraints on subviews.
- (void)setViewConstraints {
  UIView* contentView = self.contentView;

  // This view is used to center the two leading textLabels.
  UIView* verticalCenteringView = [[UIView alloc] init];
  verticalCenteringView.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:verticalCenteringView];

  _textLeadingAnchorConstraint = [_textLabel.leadingAnchor
      constraintEqualToAnchor:_imageView.trailingAnchor];
  _errorIconWidthConstraint = [_errorIcon.widthAnchor
      constraintEqualToConstant:kHorizontalErrorIconFixedSize];
  [NSLayoutConstraint activateConstraints:@[
    // Set leading anchors.
    [_imageView.leadingAnchor constraintEqualToAnchor:contentView.leadingAnchor
                                             constant:kHorizontalPadding],
    [_detailTextLabel.leadingAnchor
        constraintEqualToAnchor:_textLabel.leadingAnchor],

    // Fix image widths.
    [_imageView.widthAnchor
        constraintEqualToConstant:kHorizontalImageFixedSize],
    _errorIconWidthConstraint,

    // Set vertical anchors. This approach assumes the cell height is set by
    // the view controller. Contents are pinned to centerY, rather than pushing
    // against the top/bottom boundaries.
    [_imageView.centerYAnchor
        constraintEqualToAnchor:contentView.centerYAnchor],
    [_textLabel.topAnchor
        constraintEqualToAnchor:verticalCenteringView.topAnchor],
    [_textLabel.bottomAnchor
        constraintEqualToAnchor:_detailTextLabel.topAnchor],
    [_detailTextLabel.bottomAnchor
        constraintEqualToAnchor:verticalCenteringView.bottomAnchor],
    [verticalCenteringView.centerYAnchor
        constraintEqualToAnchor:contentView.centerYAnchor],
    [_errorIcon.centerYAnchor
        constraintEqualToAnchor:contentView.centerYAnchor],
    // Set trailing anchors.
    [_errorIcon.trailingAnchor
        constraintEqualToAnchor:contentView.trailingAnchor
                       constant:-kHorizontalPaddingBetweenImageAndText],
    [_detailTextLabel.trailingAnchor
        constraintEqualToAnchor:_errorIcon.leadingAnchor
                       constant:-kHorizontalPaddingBetweenTextAndError],
    _textLeadingAnchorConstraint,
    [_textLabel.trailingAnchor
        constraintLessThanOrEqualToAnchor:_errorIcon.leadingAnchor
                                 constant:
                                     -kHorizontalPaddingBetweenTextAndError],
  ]];

  // This is needed so the image doesn't get pushed out if both text and detail
  // are long.
  [_textLabel
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [_detailTextLabel
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisHorizontal];
}

#pragma mark - UIView

- (void)layoutSubviews {
  [super layoutSubviews];

  // Creates the image rounded corners.
  _imageView.layer.cornerRadius = _imageView.image.size.width / 2.0f;

  // Adjust the leading margin depending on existence of image.
  if (_imageView.image) {
    _textLeadingAnchorConstraint.constant =
        kHorizontalPaddingBetweenImageAndText;
  } else {
    _textLeadingAnchorConstraint.constant = 0;
  }

  if (_errorIcon.image) {
    _errorIconWidthConstraint.constant = kHorizontalErrorIconFixedSize;
  } else {
    _errorIconWidthConstraint.constant = 0;
  }
}

#pragma mark - UICollectionReusableView

- (void)prepareForReuse {
  [super prepareForReuse];
  self.imageView.image = nil;
  self.textLabel.text = nil;
  self.detailTextLabel.text = nil;
  self.textLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  self.detailTextLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  self.errorIcon.image = nil;
  self.errorIcon.tintColor = nil;
  self.accessoryType = MDCCollectionViewCellAccessoryNone;
  self.userInteractionEnabled = YES;
  self.contentView.alpha = 1;
  UIImageView* accessoryImage =
      base::mac::ObjCCastStrict<UIImageView>(self.accessoryView);
  accessoryImage.tintColor =
      [accessoryImage.tintColor colorWithAlphaComponent:1];
}

#pragma mark - NSObject(Accessibility)

- (NSString*)accessibilityLabel {
  return self.textLabel.text;
}

- (NSString*)accessibilityValue {
  return self.detailTextLabel.text;
}

@end
