// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/cells/table_view_account_item.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/settings/cells/settings_cells_constants.h"
#include "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Padding used between the image and text.
const CGFloat kHorizontalPaddingBetweenImageAndText = 10;

// Padding used between the text and error icon.
const CGFloat kHorizontalPaddingBetweenTextAndError = 5;

// Image fixed horizontal size.
const CGFloat kHorizontalImageFixedSize = 40;

// Error icon fixed horizontal size.
const CGFloat kHorizontalErrorIconFixedSize = 25;
}

@implementation TableViewAccountItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [TableViewAccountCell class];
    self.accessibilityTraits |= UIAccessibilityTraitButton;
    _mode = TableViewAccountModeEnabled;
  }
  return self;
}

#pragma mark - TableViewItem

- (void)configureCell:(TableViewAccountCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];

  cell.imageView.image = self.image;
  cell.textLabel.text = self.text;
  cell.detailTextLabel.text = self.detailText;
  if (self.shouldDisplayError) {
    cell.errorIcon.image = [[UIImage imageNamed:@"settings_error"]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    cell.errorIcon.tintColor = [UIColor colorNamed:kRedColor];
    cell.detailTextLabel.textColor = [UIColor colorNamed:kRedColor];
  } else {
    cell.errorIcon.image = nil;
    cell.detailTextLabel.textColor = UIColor.cr_secondaryLabelColor;
  }

  cell.userInteractionEnabled = self.mode == TableViewAccountModeEnabled;
  if (self.mode != TableViewAccountModeDisabled) {
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

@interface TableViewAccountCell () {
  // Constraint used to set padding between image and text when image exists.
  NSLayoutConstraint* _textLeadingAnchorConstraint;

  // Constraint used to set the errorIcon width depending on it's existence.
  NSLayoutConstraint* _errorIconWidthConstraint;
}
@end

@implementation TableViewAccountCell

@synthesize imageView = _imageView;
@synthesize textLabel = _textLabel;
@synthesize detailTextLabel = _detailTextLabel;
@synthesize errorIcon = _errorIcon;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    self.isAccessibilityElement = YES;
    [self addSubviews];
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
  _imageView.contentMode = UIViewContentModeCenter;
  _imageView.layer.masksToBounds = YES;
  _imageView.contentMode = UIViewContentModeScaleAspectFit;
  // Creates the image rounded corners.
  _imageView.layer.cornerRadius = kHorizontalImageFixedSize / 2.0f;
  [contentView addSubview:_imageView];

  _errorIcon = [[UIImageView alloc] init];
  _errorIcon.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:_errorIcon];

  _textLabel = [[UILabel alloc] init];
  _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _textLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  _textLabel.adjustsFontForContentSizeCategory = YES;
  _textLabel.textColor = UIColor.cr_labelColor;
  [contentView addSubview:_textLabel];

  _detailTextLabel = [[UILabel alloc] init];
  _detailTextLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _detailTextLabel.font =
      [UIFont preferredFontForTextStyle:kTableViewSublabelFontStyle];
  _detailTextLabel.adjustsFontForContentSizeCategory = YES;
  _detailTextLabel.textColor = UIColor.cr_secondaryLabelColor;
  [contentView addSubview:_detailTextLabel];
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
    [_imageView.leadingAnchor
        constraintEqualToAnchor:contentView.leadingAnchor
                       constant:kTableViewHorizontalSpacing],
    [_detailTextLabel.leadingAnchor
        constraintEqualToAnchor:_textLabel.leadingAnchor],

    // Fix image widths.
    [_imageView.widthAnchor
        constraintEqualToConstant:kHorizontalImageFixedSize],
    [_imageView.heightAnchor constraintEqualToAnchor:_imageView.widthAnchor],
    _errorIconWidthConstraint,

    // Set vertical anchors.
    [_imageView.centerYAnchor
        constraintEqualToAnchor:contentView.centerYAnchor],
    [_imageView.topAnchor
        constraintGreaterThanOrEqualToAnchor:contentView.topAnchor
                                    constant:kTableViewVerticalSpacing],
    [_imageView.bottomAnchor
        constraintLessThanOrEqualToAnchor:contentView.bottomAnchor
                                 constant:-kTableViewVerticalSpacing],
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
    [verticalCenteringView.topAnchor
        constraintGreaterThanOrEqualToAnchor:contentView.topAnchor
                                    constant:
                                        kTableViewTwoLabelsCellVerticalSpacing],
    [verticalCenteringView.bottomAnchor
        constraintLessThanOrEqualToAnchor:contentView.bottomAnchor
                                 constant:
                                     kTableViewTwoLabelsCellVerticalSpacing],

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

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];
  self.imageView.image = nil;
  self.textLabel.text = nil;
  self.detailTextLabel.text = nil;
  self.textLabel.textColor = UIColor.cr_labelColor;
  self.detailTextLabel.textColor = UIColor.cr_secondaryLabelColor;
  self.errorIcon.image = nil;
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

- (NSArray<NSString*>*)accessibilityUserInputLabels {
  NSMutableArray<NSString*>* userInputLabels = [[NSMutableArray alloc] init];
  if (self.textLabel.text) {
    [userInputLabels addObject:self.textLabel.text];
  }

  return userInputLabels;
}

@end
