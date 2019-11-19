// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_cell.h"

#include "base/logging.h"
#include "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/UIColor+cr_semantic_colors.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SettingsImageDetailTextCell ()

// Width constraint for the image view.
@property(nonatomic, weak, readonly) NSLayoutConstraint* imageWidthConstraint;
// Height constraint for the image view.
@property(nonatomic, weak, readonly) NSLayoutConstraint* imageHeightConstraint;
// Image view for the cell.
@property(nonatomic, strong) UIImageView* imageView;

@end

@implementation SettingsImageDetailTextCell

@synthesize textLabel = _textLabel;
@synthesize detailTextLabel = _detailTextLabel;
@synthesize imageView = _imageView;

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

// Creates and adds subviews.
- (void)addSubviews {
  UIView* contentView = self.contentView;

  _imageView = [[UIImageView alloc] init];
  _imageView.translatesAutoresizingMaskIntoConstraints = NO;
  _imageView.tintColor = UIColor.cr_labelColor;
  [contentView addSubview:_imageView];

  _textLabel = [[UILabel alloc] init];
  _textLabel.numberOfLines = 0;
  _textLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  _textLabel.adjustsFontForContentSizeCategory = YES;
  _textLabel.textColor = UIColor.cr_labelColor;

  _detailTextLabel = [[UILabel alloc] init];
  _detailTextLabel.numberOfLines = 0;
  _detailTextLabel.font =
      [UIFont preferredFontForTextStyle:kTableViewSublabelFontStyle];
  _detailTextLabel.adjustsFontForContentSizeCategory = YES;
  _detailTextLabel.textColor = UIColor.cr_secondaryLabelColor;
}

// Sets constraints on subviews.
- (void)setViewConstraints {
  UIView* contentView = self.contentView;

  UIStackView* textStackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ _textLabel, _detailTextLabel ]];
  textStackView.axis = UILayoutConstraintAxisVertical;
  textStackView.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:textStackView];

  _imageWidthConstraint = [_imageView.widthAnchor constraintEqualToConstant:0];
  _imageHeightConstraint =
      [_imageView.heightAnchor constraintEqualToConstant:0];

  [NSLayoutConstraint activateConstraints:@[
    // Horizontal contraints for |_imageView| and |textStackView|.
    _imageWidthConstraint,
    [_imageView.leadingAnchor
        constraintEqualToAnchor:contentView.leadingAnchor
                       constant:kTableViewHorizontalSpacing],
    [textStackView.leadingAnchor
        constraintEqualToAnchor:_imageView.trailingAnchor
                       constant:kTableViewHorizontalSpacing],
    [contentView.trailingAnchor
        constraintEqualToAnchor:textStackView.trailingAnchor
                       constant:kTableViewHorizontalSpacing],
    // Vertical contraints for |_imageView| and |textStackView|.
    _imageHeightConstraint,
    [_imageView.centerYAnchor
        constraintEqualToAnchor:contentView.centerYAnchor],
    [_imageView.topAnchor
        constraintGreaterThanOrEqualToAnchor:contentView.topAnchor
                                    constant:kTableViewVerticalSpacing],
    [contentView.bottomAnchor
        constraintGreaterThanOrEqualToAnchor:_imageView.bottomAnchor
                                    constant:kTableViewVerticalSpacing],
    [textStackView.centerYAnchor
        constraintEqualToAnchor:contentView.centerYAnchor],
    [textStackView.topAnchor
        constraintGreaterThanOrEqualToAnchor:contentView.topAnchor
                                    constant:
                                        kTableViewTwoLabelsCellVerticalSpacing],
    [contentView.bottomAnchor
        constraintGreaterThanOrEqualToAnchor:textStackView.bottomAnchor
                                    constant:
                                        kTableViewTwoLabelsCellVerticalSpacing],
  ]];
}

- (void)setImage:(UIImage*)image {
  DCHECK(image);
  self.imageView.image = image;
  self.imageWidthConstraint.constant = image.size.width;
  self.imageHeightConstraint.constant = image.size.height;
}

- (UIImage*)image {
  return self.imageView.image;
}

#pragma mark - UIAccessibility

- (NSString*)accessibilityLabel {
  if (self.detailTextLabel.text) {
    return [NSString stringWithFormat:@"%@, %@", self.textLabel.text,
                                      self.detailTextLabel.text];
  }
  return self.textLabel.text;
}

@end
