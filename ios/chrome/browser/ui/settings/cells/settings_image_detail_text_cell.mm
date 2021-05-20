// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_cell.h"

#include "base/check.h"
#include "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/UIColor+cr_semantic_colors.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SettingsImageDetailTextCell ()

// Image view for the cell.
@property(nonatomic, strong) UIImageView* imageView;

// Constraint used for leading text constraint without |imageView|.
@property(nonatomic, strong) NSLayoutConstraint* textNoImageConstraint;

// Constraint used for leading text constraint with |imageView| showing.
@property(nonatomic, strong) NSLayoutConstraint* textWithImageConstraint;

// Constraint used for aligning the image with the content view centerYAnchor.
@property(nonatomic, strong)
    NSLayoutConstraint* alignImageWithContentViewCenterYConstraint;

// Constraint used for aligning the image with the content view
// firstBaselineAnchor.
@property(nonatomic, strong)
    NSLayoutConstraint* alignImageWithContentViewFirstBaselineAnchorConstraint;
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
  _imageView.contentMode = UIViewContentModeCenter;
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
}

// Sets constraints on subviews.
- (void)setViewConstraints {
  UIView* contentView = self.contentView;

  UIStackView* textStackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ _textLabel, _detailTextLabel ]];
  textStackView.axis = UILayoutConstraintAxisVertical;
  textStackView.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:textStackView];

  _textNoImageConstraint = [textStackView.leadingAnchor
      constraintEqualToAnchor:contentView.leadingAnchor
                     constant:kTableViewHorizontalSpacing];

  _textWithImageConstraint = [textStackView.leadingAnchor
      constraintEqualToAnchor:_imageView.trailingAnchor
                     constant:kTableViewImagePadding];

  _alignImageWithContentViewCenterYConstraint = [_imageView.centerYAnchor
      constraintEqualToAnchor:contentView.centerYAnchor];

  _alignImageWithContentViewFirstBaselineAnchorConstraint =
      [_imageView.centerYAnchor
          constraintEqualToAnchor:contentView.firstBaselineAnchor];

  [NSLayoutConstraint activateConstraints:@[
    [_imageView.widthAnchor constraintEqualToConstant:kTableViewIconImageSize],
    [_imageView.heightAnchor constraintEqualToAnchor:_imageView.widthAnchor],
    [_imageView.leadingAnchor
        constraintEqualToAnchor:contentView.leadingAnchor
                       constant:kTableViewHorizontalSpacing],
    _alignImageWithContentViewCenterYConstraint,
    [_imageView.topAnchor
        constraintGreaterThanOrEqualToAnchor:contentView.topAnchor
                                    constant:kTableViewVerticalSpacing],
    [contentView.trailingAnchor
        constraintEqualToAnchor:textStackView.trailingAnchor
                       constant:kTableViewHorizontalSpacing],
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

    // Leading constraint for |customSepartor|.
    [self.customSeparator.leadingAnchor
        constraintEqualToAnchor:self.textLabel.leadingAnchor],
  ]];
}

- (void)setImage:(UIImage*)image {
  BOOL hidden = !image;
  self.imageView.image = image;
  self.imageView.hidden = hidden;
  // Update the leading text constraint based on |image| being provided.
  if (hidden) {
    self.textWithImageConstraint.active = NO;
    self.textNoImageConstraint.active = YES;
  } else {
    self.textNoImageConstraint.active = NO;
    self.textWithImageConstraint.active = YES;
  }
}

- (UIImage*)image {
  return self.imageView.image;
}

- (void)setImageViewTintColor:(UIColor*)color {
  _imageView.tintColor = color;
}

- (void)alignImageWithFirstLineOfText:(BOOL)alignImageWithFirstBaseline {
  if (alignImageWithFirstBaseline) {
    self.alignImageWithContentViewCenterYConstraint.active = NO;
    self.alignImageWithContentViewFirstBaselineAnchorConstraint.active = YES;
  } else {
    self.alignImageWithContentViewFirstBaselineAnchorConstraint.active = NO;
    self.alignImageWithContentViewCenterYConstraint.active = YES;
  }
}

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];
  [self alignImageWithFirstLineOfText:NO];
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
