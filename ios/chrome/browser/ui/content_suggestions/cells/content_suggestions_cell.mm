// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_cell.h"

#import "ios/chrome/browser/ui/util/i18n_string.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
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
// No image icon percentage of white.
const CGFloat kNoImageIconWhite = 0.38;
// No image background percentage of white.
const CGFloat kNoImageBackgroundWhite = 0.95;
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

// Applies the constraints on the elements. Called in the init.
- (void)applyConstraints;

@end

@implementation ContentSuggestionsCell

@synthesize titleLabel = _titleLabel;
@synthesize imageContainer = _imageContainer;
@synthesize noImageIcon = _noImageIcon;
@synthesize additionalInformationLabel = _additionalInformationLabel;
@synthesize contentImageView = _contentImageView;
@synthesize faviconView = _faviconView;
@synthesize imageSizeConstraint = _imageSizeConstraint;
@synthesize displayImage = _displayImage;

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

    _imageContainer.backgroundColor =
        [UIColor colorWithWhite:kNoImageBackgroundWhite alpha:1];
    _noImageIcon.image = [[UIImage imageNamed:kNoImageIconName]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    [_noImageIcon
        setTintColor:[UIColor colorWithWhite:kNoImageIconWhite alpha:1]];

    [[self class] configureTitleLabel:_titleLabel];
    _additionalInformationLabel.font = [[self class] additionalInformationFont];
    _faviconView.font = [[MDCTypography fontLoader] mediumFontOfSize:10];
    _additionalInformationLabel.textColor =
        [UIColor colorWithWhite:0 alpha:0.54];

    [self applyConstraints];
  }
  return self;
}

- (void)setHighlighted:(BOOL)highlighted {
  [super setHighlighted:highlighted];
  self.contentView.backgroundColor =
      highlighted ? [UIColor colorWithWhite:0 alpha:0.05] : nil;
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
  UILabel* titleLabel = [[UILabel alloc] init];
  [self configureTitleLabel:titleLabel];
  titleLabel.text = title;

  UILabel* additionalInfoLabel = [[UILabel alloc] init];
  additionalInfoLabel.font = [self additionalInformationFont];
  additionalInfoLabel.text =
      [self stringForPublisher:publisherName date:publicationDate];

  CGSize sizeForLabels =
      CGSizeMake(width - [self labelHorizontalMarginsWithImage:hasImage], 500);

  CGFloat minimalHeight = kImageSize + kStandardSpacing;

  CGFloat labelHeight = [titleLabel sizeThatFits:sizeForLabels].height;
  labelHeight += [self labelVerticalMarginsWithImage:hasImage];
  labelHeight += kSmallSpacing;
  CGFloat additionalInfoHeight =
      [additionalInfoLabel sizeThatFits:sizeForLabels].height;
  labelHeight += MAX(additionalInfoHeight, kFaviconSize);
  return MAX(minimalHeight, labelHeight);
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
      ContentSizeCategoryIsAccessibilityCategory(currentCategory);
  if (isCurrentCategoryAccessibility !=
      ContentSizeCategoryIsAccessibilityCategory(previousCategory)) {
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
      parentWidth -
      [[self class] labelHorizontalMarginsWithImage:self.displayImage];
  self.additionalInformationLabel.preferredMaxLayoutWidth =
      parentWidth - kFaviconSize - kSmallSpacing - 2 * kStandardSpacing;

  // Re-layout with the new preferred width to allow the label to adjust its
  // height.
  [super layoutSubviews];
}

#pragma mark - Private

- (void)applyConstraints {
  _imageSizeConstraint =
      [_imageContainer.heightAnchor constraintEqualToConstant:kImageSize];

  [NSLayoutConstraint activateConstraints:@[
    // Image.
    _imageSizeConstraint,
    [_imageContainer.bottomAnchor
        constraintLessThanOrEqualToAnchor:_faviconView.bottomAnchor],
    [_imageContainer.widthAnchor
        constraintEqualToAnchor:_imageContainer.heightAnchor],

    // Additional Information.
    [_additionalInformationLabel.trailingAnchor
        constraintEqualToAnchor:_titleLabel.trailingAnchor],

    // Favicon.
    [_faviconView.leadingAnchor
        constraintEqualToAnchor:_titleLabel.leadingAnchor],
    [_faviconView.centerYAnchor
        constraintEqualToAnchor:_additionalInformationLabel.centerYAnchor],
    [_faviconView.heightAnchor constraintEqualToConstant:kFaviconSize],
    [_faviconView.widthAnchor
        constraintEqualToAnchor:_faviconView.heightAnchor],
    [_faviconView.topAnchor
        constraintGreaterThanOrEqualToAnchor:self.titleLabel.bottomAnchor
                                    constant:kSmallSpacing],

    // No image icon.
    [_noImageIcon.centerXAnchor
        constraintEqualToAnchor:_imageContainer.centerXAnchor],
    [_noImageIcon.centerYAnchor
        constraintEqualToAnchor:_imageContainer.centerYAnchor],
    [_noImageIcon.widthAnchor constraintEqualToConstant:kIconSize],
    [_noImageIcon.heightAnchor constraintEqualToAnchor:_noImageIcon.widthAnchor]
  ]];

  AddSameConstraints(_contentImageView, _imageContainer);

  _imageTitleHorizontalSpacing = [_titleLabel.leadingAnchor
      constraintEqualToAnchor:_imageContainer.trailingAnchor
                     constant:kStandardSpacing];
  _imageTitleVerticalSpacing = [_titleLabel.topAnchor
      constraintEqualToAnchor:_imageContainer.bottomAnchor
                     constant:kStandardSpacing];
  _standardConstraints = @[
    _imageTitleHorizontalSpacing,
    [_titleLabel.topAnchor constraintEqualToAnchor:_imageContainer.topAnchor],
  ];

  _accessibilityConstraints = @[
    [_titleLabel.leadingAnchor
        constraintEqualToAnchor:self.contentView.leadingAnchor
                       constant:kStandardSpacing],
    _imageTitleVerticalSpacing,
  ];

  if (ContentSizeCategoryIsAccessibilityCategory(
          self.traitCollection.preferredContentSizeCategory)) {
    [NSLayoutConstraint activateConstraints:self.accessibilityConstraints];
  } else {
    [NSLayoutConstraint activateConstraints:self.standardConstraints];
  }

  ApplyVisualConstraintsWithMetrics(
      @[
        @"H:[title]-(space)-|",
        @"H:|-(space)-[image]",
        @"V:|-(space)-[image]",
        @"H:[favicon]-(small)-[additional]",
      ],
      @{
        @"image" : _imageContainer,
        @"title" : _titleLabel,
        @"additional" : _additionalInformationLabel,
        @"favicon" : _faviconView,
      },
      @{ @"space" : @(kStandardSpacing),
         @"small" : @(kSmallSpacing) });
}

+ (CGFloat)standardSpacing {
  return kStandardSpacing;
}

// Configures the |titleLabel|.
+ (void)configureTitleLabel:(UILabel*)titleLabel {
  titleLabel.textColor = [UIColor colorWithWhite:0 alpha:0.8];
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

// Returns the horizontal margin for the labels, depending if the cell
// |hasImage| and the content size category.
+ (CGFloat)labelHorizontalMarginsWithImage:(BOOL)hasImage {
  BOOL isCurrentCategoryAccessibility =
      ContentSizeCategoryIsAccessibilityCategory(
          [UIApplication sharedApplication].preferredContentSizeCategory);

  CGFloat offset = (hasImage && !isCurrentCategoryAccessibility)
                       ? kImageSize + kStandardSpacing
                       : 0;
  return 2 * kStandardSpacing + offset;
}

// Returns the vertical margin for the labels, depending if the cell |hasImage|
// and the content size category.
+ (CGFloat)labelVerticalMarginsWithImage:(BOOL)hasImage {
  BOOL isCurrentCategoryAccessibility =
      ContentSizeCategoryIsAccessibilityCategory(
          [UIApplication sharedApplication].preferredContentSizeCategory);

  CGFloat offset = (hasImage && isCurrentCategoryAccessibility)
                       ? kImageSize + kStandardSpacing
                       : 0;
  return kStandardSpacing + offset;
}

// Returns the attributed string to be displayed.
+ (NSString*)stringForPublisher:(NSString*)publisherName date:(NSString*)date {
  return AdjustStringForLocaleDirection(
      [NSString stringWithFormat:@"%@ • %@ ", publisherName, date]);
}

@end
