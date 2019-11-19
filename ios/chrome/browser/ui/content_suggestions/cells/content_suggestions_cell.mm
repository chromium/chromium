// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_cell.h"

#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/i18n_string.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#import "ios/chrome/common/favicon/favicon_view.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kImageSize = 80;
const CGFloat kStandardSpacing = 13;
const CGFloat kSmallSpacing = 7;

// Size of the favicon view.
const CGFloat kFaviconSize = 16;
// Size of the icon displayed when there is not image but one should be
// displayed.
const CGFloat kIconSize = 24;
// Name of the icon displayed when there is not image but one should be
// displayed.
NSString* const kNoImageIconName = @"content_suggestions_no_image";
// Duration of the animation to display the image.
const CGFloat kAnimationDuration = 0.3;
}

@interface ContentSuggestionsCell ()

@property(nonatomic, strong) UILabel* additionalInformationLabel;
// Contains the no-image icon or the image.
@property(nonatomic, strong) UIView* imageContainer;
// The no-image icon displayed when there is no image.
@property(nonatomic, strong) UIImageView* noImageIcon;
// Displays the image associated with this suggestion. It is added to the
// imageContainer only if there is an image to display, hiding the no-image
// icon.
@property(nonatomic, strong) UIImageView* contentImageView;
// Constraint for the size of the image.
@property(nonatomic, strong) NSLayoutConstraint* imageSizeConstraint;
// Constraint for the horizontal distance between the texts and the image
// (standard content size).
@property(nonatomic, strong) NSLayoutConstraint* imageTitleHorizontalSpacing;
// Constraint for the vertical distance between the texts and the image
// (accessibility content size).
@property(nonatomic, strong) NSLayoutConstraint* imageTitleVerticalSpacing;

// When they are activated, the image is on the leading side of the text.
// They conflict with the accessibilityConstraints.
@property(nonatomic, strong) NSArray<NSLayoutConstraint*>* standardConstraints;
// When they are activated, the image is above the text. The text is taking the
// full width. They conflict with the standardConstraints.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* accessibilityConstraints;

@end

@implementation ContentSuggestionsCell

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _titleLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _imageContainer = [[UIView alloc] initWithFrame:CGRectZero];
    _imageContainer.layer.cornerRadius = 11;
    _imageContainer.layer.masksToBounds = YES;
    _noImageIcon = [[UIImageView alloc] initWithFrame:CGRectZero];
    _additionalInformationLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _contentImageView = [[UIImageView alloc] initWithFrame:CGRectZero];
    _faviconView = [[FaviconView alloc] init];

    _contentImageView.contentMode = UIViewContentModeScaleAspectFill;
    _contentImageView.clipsToBounds = YES;
    _contentImageView.hidden = YES;

    _imageContainer.translatesAutoresizingMaskIntoConstraints = NO;
    _noImageIcon.translatesAutoresizingMaskIntoConstraints = NO;
    _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _additionalInformationLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _contentImageView.translatesAutoresizingMaskIntoConstraints = NO;
    _faviconView.translatesAutoresizingMaskIntoConstraints = NO;

    [self.contentView addSubview:_imageContainer];
    [self.contentView addSubview:_titleLabel];
    [self.contentView addSubview:_additionalInformationLabel];
    [self.contentView addSubview:_faviconView];

    [_imageContainer addSubview:_noImageIcon];
    [_imageContainer addSubview:_contentImageView];

    _imageContainer.backgroundColor = [UIColor colorNamed:kGrey100Color];
    _noImageIcon.image = [[UIImage imageNamed:kNoImageIconName]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    _noImageIcon.tintColor = [UIColor colorNamed:kGrey400Color];

    [[self class] configureTitleLabel:_titleLabel];
    _additionalInformationLabel.font = [[self class] additionalInformationFont];
    _faviconView.font = [[MDCTypography fontLoader] mediumFontOfSize:10];
    _additionalInformationLabel.textColor =
        [UIColor colorNamed:kTextSecondaryColor];

    [self applyConstraints];
  }
  return self;
}

- (void)setHighlighted:(BOOL)highlighted {
  [super setHighlighted:highlighted];
  self.contentView.backgroundColor =
      highlighted ? [UIColor colorNamed:kTableViewRowHighlightColor] : nil;
}

- (void)setContentImage:(UIImage*)image animated:(BOOL)animated {
  if (!image) {
    self.contentImageView.hidden = YES;
    return;
  }

  self.contentImageView.image = image;
  self.contentImageView.hidden = NO;

  if (!animated) {
    return;
  }

  self.contentImageView.alpha = 0;

  [UIView animateWithDuration:kAnimationDuration
                   animations:^{
                     self.contentImageView.alpha = 1;
                   }];
}

- (void)setAdditionalInformationWithPublisherName:(NSString*)publisherName
                                             date:(NSString*)date {
  self.additionalInformationLabel.text =
      [[self class] stringForPublisher:publisherName date:date];
}

- (void)setDisplayImage:(BOOL)displayImage {
  if (displayImage) {
    self.imageTitleHorizontalSpacing.constant = kStandardSpacing;
    self.imageTitleVerticalSpacing.constant = kStandardSpacing;
    self.imageSizeConstraint.constant = kImageSize;
    self.imageContainer.hidden = NO;
  } else {
    self.imageTitleHorizontalSpacing.constant = 0;
    self.imageTitleVerticalSpacing.constant = 0;
    self.imageSizeConstraint.constant = 0;
    self.imageContainer.hidden = YES;
  }
  _displayImage = displayImage;
}

+ (CGFloat)heightForWidth:(CGFloat)width
       withImageAvailable:(BOOL)hasImage
                    title:(NSString*)title
            publisherName:(NSString*)publisherName
          publicationDate:(NSString*)publicationDate {
  // Calculate title height.
  UILabel* titleLabel = [[UILabel alloc] init];
  [self configureTitleLabel:titleLabel];
  titleLabel.text = title;
  CGSize spaceForTitle = CGSizeMake(
      [self availableWidthForTitleInWidth:width withImage:hasImage], 500);
  CGFloat titleHeight = [titleLabel sizeThatFits:spaceForTitle].height;

  // Calculate subtitle height.
  UILabel* additionalInfoLabel = [[UILabel alloc] init];
  additionalInfoLabel.font = [self additionalInformationFont];
  additionalInfoLabel.text =
      [self stringForPublisher:publisherName date:publicationDate];
  CGSize spaceForAdditionalInfo = CGSizeMake(
      [self availableWidthForAdditionalInfoInWidth:width withImage:hasImage],
      500);
  CGFloat additionalInfoHeight =
      [additionalInfoLabel sizeThatFits:spaceForAdditionalInfo].height;
  // The height of the line containing the favicon and the additional info.
  CGFloat subtitleHeight = MAX(kFaviconSize, additionalInfoHeight);

  // Calculate final height. Add up heights from top to bottom.
  BOOL isCurrentCategoryAccessibility =
      UIContentSizeCategoryIsAccessibilityCategory(
          UIApplication.sharedApplication.preferredContentSizeCategory);
  if (isCurrentCategoryAccessibility || !hasImage) {
    return kStandardSpacing + (hasImage ? kImageSize + kStandardSpacing : 0) +
           titleHeight + kSmallSpacing + subtitleHeight + kStandardSpacing;
  } else {
    CGFloat leftSideHeight = kStandardSpacing + titleHeight + kSmallSpacing +
                             subtitleHeight + kStandardSpacing;
    CGFloat rightSideHeight = kStandardSpacing + kImageSize + kStandardSpacing;
    return MAX(leftSideHeight, rightSideHeight);
  }
}

#pragma mark - UICollectionViewCell

- (void)prepareForReuse {
  [super prepareForReuse];
  self.titleLabel.text = nil;
  self.displayImage = NO;
}

#pragma mark - UIView

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  UIContentSizeCategory currentCategory =
      self.traitCollection.preferredContentSizeCategory;
  UIContentSizeCategory previousCategory =
      previousTraitCollection.preferredContentSizeCategory;

  BOOL isCurrentCategoryAccessibility =
      UIContentSizeCategoryIsAccessibilityCategory(currentCategory);
  if (isCurrentCategoryAccessibility !=
      UIContentSizeCategoryIsAccessibilityCategory(previousCategory)) {
    if (isCurrentCategoryAccessibility) {
      [NSLayoutConstraint deactivateConstraints:self.standardConstraints];
      [NSLayoutConstraint activateConstraints:self.accessibilityConstraints];
    } else {
      [NSLayoutConstraint deactivateConstraints:self.accessibilityConstraints];
      [NSLayoutConstraint activateConstraints:self.standardConstraints];
    }
  }
  if (currentCategory != previousCategory) {
    [[self class] configureTitleLabel:_titleLabel];
    _additionalInformationLabel.font = [[self class] additionalInformationFont];
  }
}

// Implements -layoutSubviews as per instructions in documentation for
// +[MDCCollectionViewCell cr_preferredHeightForWidth:forItem:].
- (void)layoutSubviews {
  [super layoutSubviews];

  // Adjust the text label preferredMaxLayoutWidth when the parent's width
  // changes, for instance on screen rotation.
  CGFloat parentWidth = CGRectGetWidth(self.contentView.bounds);

  self.titleLabel.preferredMaxLayoutWidth =
      [[self class] availableWidthForTitleInWidth:parentWidth
                                        withImage:self.displayImage];
  self.additionalInformationLabel.preferredMaxLayoutWidth =
      [[self class] availableWidthForAdditionalInfoInWidth:parentWidth
                                                 withImage:self.displayImage];

  // Re-layout with the new preferred width to allow the label to adjust its
  // height.
  [super layoutSubviews];
}

#pragma mark - Private

// Applies the constraints on the elements. Called in the init.
- (void)applyConstraints {
  _imageSizeConstraint =
      [_imageContainer.heightAnchor constraintEqualToConstant:kImageSize];

  [NSLayoutConstraint activateConstraints:@[
    // Image.
    _imageSizeConstraint,
    [_imageContainer.widthAnchor
        constraintEqualToAnchor:_imageContainer.heightAnchor],
    [_imageContainer.topAnchor
        constraintEqualToAnchor:self.contentView.topAnchor
                       constant:kStandardSpacing],

    // Favicon.
    [_faviconView.leadingAnchor
        constraintEqualToAnchor:_titleLabel.leadingAnchor],
    [_faviconView.heightAnchor constraintEqualToConstant:kFaviconSize],
    [_faviconView.widthAnchor
        constraintEqualToAnchor:_faviconView.heightAnchor],
    [_faviconView.topAnchor
        constraintGreaterThanOrEqualToAnchor:_titleLabel.bottomAnchor
                                    constant:kSmallSpacing],
    [_faviconView.bottomAnchor
        constraintGreaterThanOrEqualToAnchor:_imageContainer.bottomAnchor],

    // Additional Information.
    [_additionalInformationLabel.leadingAnchor
        constraintEqualToAnchor:_faviconView.trailingAnchor
                       constant:kSmallSpacing],
    [_additionalInformationLabel.trailingAnchor
        constraintEqualToAnchor:_titleLabel.trailingAnchor],
    [_additionalInformationLabel.topAnchor
        constraintGreaterThanOrEqualToAnchor:_titleLabel.bottomAnchor
                                    constant:kSmallSpacing],
    [_additionalInformationLabel.bottomAnchor
        constraintGreaterThanOrEqualToAnchor:_imageContainer.bottomAnchor],

    // Align favicon with additional information.
    [_additionalInformationLabel.centerYAnchor
        constraintEqualToAnchor:_faviconView.centerYAnchor],

    // No image icon.
    [_noImageIcon.centerXAnchor
        constraintEqualToAnchor:_imageContainer.centerXAnchor],
    [_noImageIcon.centerYAnchor
        constraintEqualToAnchor:_imageContainer.centerYAnchor],
    [_noImageIcon.widthAnchor constraintEqualToConstant:kIconSize],
    [_noImageIcon.heightAnchor constraintEqualToAnchor:_noImageIcon.widthAnchor]
  ]];

  AddSameConstraints(_contentImageView, _imageContainer);

  // Constraints for regular font size.
  _imageTitleHorizontalSpacing = [_imageContainer.leadingAnchor
      constraintEqualToAnchor:_titleLabel.trailingAnchor
                     constant:kStandardSpacing];
  _standardConstraints = @[
    _imageTitleHorizontalSpacing,
    [_titleLabel.topAnchor constraintEqualToAnchor:_imageContainer.topAnchor],
    [_titleLabel.leadingAnchor
        constraintEqualToAnchor:self.contentView.leadingAnchor
                       constant:kStandardSpacing],
    [_imageContainer.trailingAnchor
        constraintEqualToAnchor:self.contentView.trailingAnchor
                       constant:-kStandardSpacing],
  ];

  // Constraints for a11y font size.
  _imageTitleVerticalSpacing = [_titleLabel.topAnchor
      constraintEqualToAnchor:_imageContainer.bottomAnchor
                     constant:kStandardSpacing];
  _accessibilityConstraints = @[
    _imageTitleVerticalSpacing,
    [_imageContainer.leadingAnchor
        constraintEqualToAnchor:self.contentView.leadingAnchor
                       constant:kStandardSpacing],
    [_titleLabel.leadingAnchor
        constraintEqualToAnchor:_imageContainer.leadingAnchor],
    [_titleLabel.trailingAnchor
        constraintEqualToAnchor:self.contentView.trailingAnchor
                       constant:-kStandardSpacing],
  ];

  if (UIContentSizeCategoryIsAccessibilityCategory(
          self.traitCollection.preferredContentSizeCategory)) {
    [NSLayoutConstraint activateConstraints:self.accessibilityConstraints];
  } else {
    [NSLayoutConstraint activateConstraints:self.standardConstraints];
  }
}

+ (CGFloat)standardSpacing {
  return kStandardSpacing;
}

// Configures the |titleLabel|.
+ (void)configureTitleLabel:(UILabel*)titleLabel {
  titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  UIFontDescriptor* descriptor = [[UIFontDescriptor
      preferredFontDescriptorWithTextStyle:UIFontTextStyleSubheadline]
      fontDescriptorWithSymbolicTraits:UIFontDescriptorTraitBold];
  titleLabel.font = [UIFont fontWithDescriptor:descriptor size:0];
  titleLabel.numberOfLines = 3;
}

// Returns the font used to display the additional informations.
+ (UIFont*)additionalInformationFont {
  return [UIFont preferredFontForTextStyle:UIFontTextStyleCaption2];
}

// Returns the available width for main title within |width|.
+ (CGFloat)availableWidthForTitleInWidth:(CGFloat)width
                               withImage:(BOOL)hasImage {
  BOOL isCurrentCategoryAccessibility =
      UIContentSizeCategoryIsAccessibilityCategory(
          UIApplication.sharedApplication.preferredContentSizeCategory);
  // If |hasImage|==YES and current font size is not a11y, the image will be
  // displayed in the same vertical level with main title. Subtract the image's
  // width and the margin between it and the main title.
  width -= (hasImage && !isCurrentCategoryAccessibility)
               ? kImageSize + kStandardSpacing
               : 0;
  // Subtract the margin on both sides.
  return width - 2 * kStandardSpacing;
}

+ (CGFloat)availableWidthForAdditionalInfoInWidth:(CGFloat)width
                                        withImage:(BOOL)hasImage {
  width = [self availableWidthForTitleInWidth:width withImage:hasImage];
  // Subtract the favicon's width and the margin between it and the
  // AdditionalInfo.
  return width - kFaviconSize - kSmallSpacing;
}

// Returns the attributed string to be displayed.
+ (NSString*)stringForPublisher:(NSString*)publisherName date:(NSString*)date {
  return AdjustStringForLocaleDirection(
      [NSString stringWithFormat:@"%@ • %@ ", publisherName, date]);
}

@end
