// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_cell.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"

namespace {

// Value of the constant used to provide an alignment on the X center of the
// images in different rows.
const CGFloat kImageXCenterAlignmentOffset = 14;

}  // namespace

@interface SettingsImageDetailTextCell ()

// Image view for the cell.
@property(nonatomic, strong) UIImageView* imageView;

// Constraint used for leading text constraint without `imageView`.
@property(nonatomic, strong) NSLayoutConstraint* textNoImageConstraint;

// Constraint used for leading text constraint with `imageView` showing.
@property(nonatomic, strong) NSLayoutConstraint* textWithImageConstraint;

// Constraint used for aligning the image with the content view centerYAnchor.
@property(nonatomic, strong)
    NSLayoutConstraint* alignImageWithContentViewCenterYConstraint;

// Constraint used for aligning the image with the content view
// firstBaselineAnchor.
@property(nonatomic, strong)
    NSLayoutConstraint* alignImageWithContentViewFirstBaselineAnchorConstraint;

// Constraint between the image and the text that is used to center the images
// in different rows. Only active when there is an image.
@property(nonatomic, strong) NSLayoutConstraint* imageTextAlignmentConstraint;

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
  _imageView.tintColor = [UIColor colorNamed:kTextPrimaryColor];
  [_imageView setContentHuggingPriority:UILayoutPriorityRequired
                                forAxis:UILayoutConstraintAxisHorizontal];
  [_imageView
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [contentView addSubview:_imageView];

  _textLabel = [[UILabel alloc] init];
  _textLabel.numberOfLines = 0;
  _textLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  _textLabel.adjustsFontForContentSizeCategory = YES;
  _textLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];

  _detailTextLabel = [[UILabel alloc] init];
  _detailTextLabel.numberOfLines = 0;
  _detailTextLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
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
      constraintGreaterThanOrEqualToAnchor:_imageView.trailingAnchor
                                  constant:kTableViewImagePadding];

  _alignImageWithContentViewCenterYConstraint = [_imageView.centerYAnchor
      constraintEqualToAnchor:contentView.centerYAnchor];

  _alignImageWithContentViewFirstBaselineAnchorConstraint =
      [_imageView.firstBaselineAnchor
          constraintEqualToAnchor:contentView.firstBaselineAnchor];

  // To ensure the images are aligned in different rows even if they have
  // different width.
  NSLayoutConstraint* imageLeadingAlignmentConstraint =
      [_imageView.centerXAnchor
          constraintEqualToAnchor:contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing +
                                  kImageXCenterAlignmentOffset];
  imageLeadingAlignmentConstraint.priority = UILayoutPriorityDefaultHigh + 1;
  imageLeadingAlignmentConstraint.active = YES;

  _imageTextAlignmentConstraint = [_imageView.centerXAnchor
      constraintEqualToAnchor:contentView.leadingAnchor
                     constant:kTableViewHorizontalSpacing +
                              kImageXCenterAlignmentOffset];
  _imageTextAlignmentConstraint.priority = UILayoutPriorityDefaultHigh + 1;

  [NSLayoutConstraint activateConstraints:@[
    [_imageView.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:contentView.leadingAnchor
                                    constant:kTableViewHorizontalSpacing],
    _alignImageWithContentViewCenterYConstraint,
    [_imageView.topAnchor
        constraintGreaterThanOrEqualToAnchor:contentView.topAnchor
                                    constant:
                                        kTableViewTwoLabelsCellVerticalSpacing],
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

    // Leading constraint for `customSepartor`.
    [self.customSeparator.leadingAnchor
        constraintEqualToAnchor:self.textLabel.leadingAnchor],
  ]];
}

- (void)setImage:(UIImage*)image {
  BOOL hidden = !image;
  self.imageView.image = image;
  self.imageView.hidden = hidden;
  // Update the leading text constraint based on `image` being provided.
  if (hidden) {
    self.imageTextAlignmentConstraint.active = NO;
    self.textWithImageConstraint.active = NO;
    self.textNoImageConstraint.active = YES;
  } else {
    self.textNoImageConstraint.active = NO;
    self.imageTextAlignmentConstraint.active = YES;
    self.textWithImageConstraint.active = YES;
  }
}

- (UIImage*)image {
  return self.imageView.image;
}

- (void)setImageViewAlpha:(CGFloat)alpha {
  _imageView.alpha = alpha;
}

- (void)setImageViewTintColor:(UIColor*)color {
  _imageView.tintColor = color;
}

- (void)alignImageWithFirstLineOfText:(BOOL)alignImageWithFirstBaseline {
  self.alignImageWithContentViewCenterYConstraint.active =
      !alignImageWithFirstBaseline;
  self.alignImageWithContentViewFirstBaselineAnchorConstraint.active =
      alignImageWithFirstBaseline;
}

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];
  [self alignImageWithFirstLineOfText:NO];
  _imageView.alpha = 1.0f;
}

#pragma mark - UIAccessibility

- (NSString*)accessibilityLabel {
  if (!self.textLabel.text) {
    return self.detailTextLabel.text;
  }

  if (self.detailTextLabel.text) {
    return [NSString stringWithFormat:@"%@, %@", self.textLabel.text,
                                      self.detailTextLabel.text];
  }

  return self.textLabel.text;
}

- (NSArray<NSString*>*)accessibilityUserInputLabels {
  // The name for Voice Control includes only `self.textLabel.text`.
  if (!self.textLabel.text) {
    return @[];
  }
  return @[ self.textLabel.text ];
}

@end
