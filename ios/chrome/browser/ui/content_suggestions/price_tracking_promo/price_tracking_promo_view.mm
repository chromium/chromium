// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_view.h"

#import "base/i18n/rtl.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_item.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/gradient_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Spacing between items stacked vertically (title, description and allow label)
const CGFloat kVerticalStackSpacing = 15.0f;

// Spacing between items stacked horizontally (product image and text stack
// (which contains title, description and allow label)).
const CGFloat kHorizontalStackSpacing = 16.0f;

// Alpha for top of gradient overlay.
const CGFloat kGradientOverlayTopAlpha = 0.0;

// Alpha for bottom of gradienet overlay.
const CGFloat kGradientOverlayBottomAlpha = 0.14;

// Radius of background circle of product image fallback.
const CGFloat kProductImageFallbackCornerRadius = 18.0;

// Vertical margin between down trend symbol and circle around it.
const CGFloat kProductImageFallbackVerticalMargin = 6.0;

// Horizontal margin between down trend symbol and circle around it.
const CGFloat kProductImageFallbackHorizontalMargin = 4.0;

// Width and height of product image fallback
const CGFloat kProductImageFallbackSize = 36.0;

// Rounded corners of the product image radius
const CGFloat kProductImageCornerRadius = 8.0;

// Width and height of product image.
const CGFloat kProductImageWidthHeight = 48.0;

// Separator height.
const CGFloat kSeparatorHeight = 0.5;

// Properties for Favicon image.
const CGFloat kFaviconImageViewCornerRadius = 2.0;
const CGFloat kFaviconImageViewTrailingCornerRadius = 4.0;
const CGFloat kFaviconImageViewHeightWidth = 10.0;

// Properties for Favicon image container.
const CGFloat kFaviconImageContainerCornerRadius = 3.0;
const CGFloat kFaviconImageContainerTrailingCornerRadius = 6.0;
const CGFloat kFaviconImageContainerHeightWidth = 15.0;
const CGFloat kFaviconImageContainerTrailingMargin = -4.62;

}  // namespace

@implementation PriceTrackingPromoModuleView {
  UILabel* _titleLabel;
  UILabel* _descriptionLabel;
  UIButton* _allowButton;
  UIImageView* _productImageView;
  UIImageView* _faviconImageView;
  UIView* _faviconImageContainer;
  // To create a background circle around the fallback product image.
  UIImageView* _fallbackProductImageView;
  UIView* _productImage;
  UIView* _gradientOverlay;
  UIStackView* _contentStack;
  UIStackView* _textStack;
  UIView* _separator;
  UITapGestureRecognizer* _tapRecognizer;
}

// Create a mask with radius applied to the trailing corner
- (CAShapeLayer*)faviconMaskWithRadius:(CGFloat)trailingCornerRadius
                      imageHeightWidth:(CGFloat)imageHeightWidth {
  UIRectCorner bottomTrail =
      base::i18n::IsRTL() ? UIRectCornerBottomLeft : UIRectCornerBottomRight;
  CGSize cornerRadiusSize =
      CGSizeMake(trailingCornerRadius, trailingCornerRadius);
  UIBezierPath* bezierPath =
      [UIBezierPath bezierPathWithRoundedRect:CGRectMake(0, 0, imageHeightWidth,
                                                         imageHeightWidth)
                            byRoundingCorners:bottomTrail
                                  cornerRadii:cornerRadiusSize];
  CAShapeLayer* mask = [CAShapeLayer layer];
  mask.path = bezierPath.CGPath;
  return mask;
}

- (void)configureView:(PriceTrackingPromoItem*)config {
  if (!config) {
    return;
  }
  if (!(self.subviews.count == 0)) {
    return;
  }
  self.translatesAutoresizingMaskIntoConstraints = NO;
  self.accessibilityIdentifier = kPriceTrackingPromoViewID;
  _titleLabel = [[UILabel alloc] init];
  _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  _titleLabel.numberOfLines = 1;
  _titleLabel.lineBreakMode = NSLineBreakByTruncatingTail;

  _titleLabel.font =
      CreateDynamicFont(UIFontTextStyleFootnote, UIFontWeightSemibold, self);
  _titleLabel.adjustsFontForContentSizeCategory = YES;
  _titleLabel.text = l10n_util::GetNSString(
      IDS_IOS_CONTENT_SUGGESTIONS_PRICE_TRACKING_PROMO_TITLE);
  _titleLabel.isAccessibilityElement = YES;
  _titleLabel.accessibilityTraits |= UIAccessibilityTraitHeader;

  _descriptionLabel = [[UILabel alloc] init];
  _descriptionLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _descriptionLabel.numberOfLines = 2;
  _descriptionLabel.lineBreakMode = NSLineBreakByTruncatingTail;
  _descriptionLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  _descriptionLabel.adjustsFontForContentSizeCategory = YES;
  _descriptionLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  _descriptionLabel.text = l10n_util::GetNSString(
      IDS_IOS_CONTENT_SUGGESTIONS_PRICE_TRACKING_PROMO_DESCRIPTION);
  _descriptionLabel.isAccessibilityElement = YES;

  // TODO(crbug.com/361106168) use product image from most recent subscription
  // if available.

  _productImage = [[UIView alloc] init];
  UIImage* retrievedProductImage =
      [UIImage imageWithData:config.productImageData
                       scale:[UIScreen mainScreen].scale];
  if (retrievedProductImage) {
    _productImageView = [[UIImageView alloc] init];
    _productImageView.image = retrievedProductImage;
    _productImageView.contentMode = UIViewContentModeScaleAspectFill;
    _productImageView.translatesAutoresizingMaskIntoConstraints = NO;
    _productImageView.layer.borderWidth = 0;
    _productImageView.layer.cornerRadius = kProductImageCornerRadius;
    _productImageView.layer.masksToBounds = YES;
    _productImageView.backgroundColor = UIColor.whiteColor;

    _gradientOverlay = [[GradientView alloc]
        initWithTopColor:[[UIColor blackColor]
                             colorWithAlphaComponent:kGradientOverlayTopAlpha]
             bottomColor:
                 [[UIColor blackColor]
                     colorWithAlphaComponent:kGradientOverlayBottomAlpha]];
    _gradientOverlay.translatesAutoresizingMaskIntoConstraints = NO;
    _gradientOverlay.layer.cornerRadius = kProductImageCornerRadius;
    _gradientOverlay.layer.zPosition = 1;

    if (config.faviconImage) {
      _faviconImageContainer = [[UIView alloc] init];
      _faviconImageContainer.translatesAutoresizingMaskIntoConstraints = NO;
      _faviconImageContainer.layer.borderWidth = 0;
      _faviconImageContainer.backgroundColor =
          [UIColor colorNamed:kBackgroundColor];
      _faviconImageContainer.layer.cornerRadius =
          kFaviconImageContainerCornerRadius;
      _faviconImageContainer.layer.masksToBounds = YES;
      // Apply bottom right radius mask
      _faviconImageContainer.layer.mask =
          [self faviconMaskWithRadius:kFaviconImageContainerTrailingCornerRadius
                     imageHeightWidth:kFaviconImageContainerHeightWidth];

      _faviconImageView = [[UIImageView alloc] init];
      _faviconImageView.image = config.faviconImage;
      _faviconImageView.contentMode = UIViewContentModeScaleAspectFill;
      _faviconImageView.translatesAutoresizingMaskIntoConstraints = NO;
      _faviconImageView.layer.borderWidth = 0;
      _faviconImageView.layer.cornerRadius = kFaviconImageViewCornerRadius;
      _faviconImageView.layer.masksToBounds = YES;
      _faviconImageView.backgroundColor = [UIColor colorNamed:kBackgroundColor];
      // Apply bottom right radius mask
      _faviconImageView.layer.mask =
          [self faviconMaskWithRadius:kFaviconImageViewTrailingCornerRadius
                     imageHeightWidth:kFaviconImageViewHeightWidth];
    }

    [_productImage addSubview:_productImageView];
    [_productImageView addSubview:_gradientOverlay];
    if (_faviconImageContainer) {
      [_productImage addSubview:_faviconImageContainer];
      [_faviconImageContainer addSubview:_faviconImageView];
    }

    NSMutableArray* constraints = [[NSMutableArray alloc] init];
    [constraints addObjectsFromArray:@[
      [_productImage.heightAnchor
          constraintEqualToConstant:kProductImageWidthHeight],
      [_productImage.widthAnchor
          constraintEqualToAnchor:_productImage.heightAnchor],
      [_productImageView.heightAnchor
          constraintEqualToConstant:kProductImageWidthHeight],
      [_productImageView.widthAnchor
          constraintEqualToAnchor:_productImageView.heightAnchor],
      [_gradientOverlay.heightAnchor
          constraintEqualToConstant:kProductImageWidthHeight],
      [_gradientOverlay.widthAnchor
          constraintEqualToAnchor:_gradientOverlay.heightAnchor],
    ]];
    if (_faviconImageContainer) {
      [constraints addObjectsFromArray:@[
        [_faviconImageContainer.heightAnchor
            constraintEqualToConstant:kFaviconImageContainerHeightWidth],
        [_faviconImageContainer.widthAnchor
            constraintEqualToAnchor:_faviconImageContainer.heightAnchor],
        [_faviconImageContainer.trailingAnchor
            constraintEqualToAnchor:_productImage.trailingAnchor
                           constant:kFaviconImageContainerTrailingMargin],
        [_faviconImageContainer.bottomAnchor
            constraintEqualToAnchor:_productImage.bottomAnchor
                           constant:kFaviconImageContainerTrailingMargin],
        [_faviconImageView.heightAnchor
            constraintEqualToConstant:kFaviconImageViewHeightWidth],
        [_faviconImageView.widthAnchor
            constraintEqualToAnchor:_faviconImageView.heightAnchor],
        [_faviconImageView.centerXAnchor
            constraintEqualToAnchor:_faviconImageContainer.centerXAnchor],
        [_faviconImageView.centerYAnchor
            constraintEqualToAnchor:_faviconImageContainer.centerYAnchor],
      ]];
    }

    [NSLayoutConstraint activateConstraints:constraints];

  } else {
    _fallbackProductImageView = [[UIImageView alloc] init];
    UIImageSymbolConfiguration* fallbackImageConfig =
        [UIImageSymbolConfiguration
            configurationWithWeight:UIImageSymbolWeightLight];
    _fallbackProductImageView.image =
        CustomSymbolWithConfiguration(kDownTrendSymbol, fallbackImageConfig);
    _fallbackProductImageView.contentMode = UIViewContentModeScaleAspectFit;
    _fallbackProductImageView.translatesAutoresizingMaskIntoConstraints = NO;
    _fallbackProductImageView.layer.borderWidth = 0;

    [NSLayoutConstraint activateConstraints:@[
      [_productImage.heightAnchor
          constraintEqualToConstant:kProductImageFallbackSize],
      [_productImage.widthAnchor
          constraintEqualToConstant:kProductImageFallbackSize],
    ]];

    _productImage.layer.cornerRadius = kProductImageFallbackCornerRadius;
    _productImage.backgroundColor = [UIColor colorNamed:kBlueHaloColor];

    [_productImage addSubview:_fallbackProductImageView];

    AddSameConstraintsWithInsets(
        _fallbackProductImageView, _productImage,
        NSDirectionalEdgeInsets{kProductImageFallbackVerticalMargin,
                                kProductImageFallbackHorizontalMargin,
                                kProductImageFallbackVerticalMargin,
                                kProductImageFallbackHorizontalMargin});
  }

  _allowButton = [[UIButton alloc] init];
  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  buttonConfiguration.contentInsets = NSDirectionalEdgeInsetsZero;
  buttonConfiguration.titleLineBreakMode = NSLineBreakByTruncatingTail;
  buttonConfiguration.attributedTitle = [[NSAttributedString alloc]
      initWithString:l10n_util::GetNSString(
                         IDS_IOS_CONTENT_SUGGESTIONS_PRICE_TRACKING_PROMO_ALLOW)
          attributes:@{
            NSFontAttributeName :
                [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote]
          }];
  _allowButton.configuration = buttonConfiguration;
  [_allowButton setTitleColor:[UIColor colorNamed:kBlueColor]
                     forState:UIControlStateNormal];
  _allowButton.isAccessibilityElement = YES;
  _allowButton.titleLabel.numberOfLines = 1;
  _allowButton.titleLabel.adjustsFontForContentSizeCategory = YES;

  _allowButton.contentHorizontalAlignment =
      UIControlContentHorizontalAlignmentTrailing;
  _allowButton.accessibilityIdentifier = _allowButton.titleLabel.text;
  _tapRecognizer = [[UITapGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(allowPriceTrackingTapped:)];
  [_allowButton addGestureRecognizer:_tapRecognizer];

  _separator = [[UIView alloc] init];
  _separator.backgroundColor = [UIColor colorNamed:kSeparatorColor];

  _textStack = [[UIStackView alloc] initWithArrangedSubviews:@[
    _titleLabel, _descriptionLabel, _separator, _allowButton
  ]];
  _textStack.axis = UILayoutConstraintAxisVertical;
  _textStack.translatesAutoresizingMaskIntoConstraints = NO;
  _textStack.alignment = UIStackViewAlignmentLeading;
  _textStack.spacing = kVerticalStackSpacing;

  [NSLayoutConstraint activateConstraints:@[
    [_separator.heightAnchor
        constraintEqualToConstant:AlignValueToPixel(kSeparatorHeight)],
    [_separator.leadingAnchor constraintEqualToAnchor:_textStack.leadingAnchor],
    [_separator.trailingAnchor
        constraintEqualToAnchor:_textStack.trailingAnchor],
  ]];

  _contentStack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ _productImage, _textStack ]];
  _contentStack.translatesAutoresizingMaskIntoConstraints = NO;
  _contentStack.spacing = kHorizontalStackSpacing;
  _contentStack.alignment = UIStackViewAlignmentTop;
  [self addSubview:_contentStack];
  AddSameConstraints(_contentStack, self);
  if (@available(iOS 17, *)) {
    NSArray<UITrait>* traits = TraitCollectionSetForTraits(
        @[ UITraitPreferredContentSizeCategory.class ]);
    [self registerForTraitChanges:traits
                       withAction:@selector(hideDescriptionOnTraitChange)];
  }
}

- (void)allowPriceTrackingTapped:(UIGestureRecognizer*)sender {
  [self.commandHandler allowPriceTrackingNotifications];
}

- (void)hideDescriptionOnTraitChange {
  _descriptionLabel.hidden = self.traitCollection.preferredContentSizeCategory >
                             UIContentSizeCategoryExtraExtraLarge;
}

#pragma mark - Testing category methods

- (NSString*)titleLabelTextForTesting {
  return self->_titleLabel.text;
}

- (NSString*)descriptionLabelTextForTesting {
  return self->_descriptionLabel.text;
}

- (NSString*)allowLabelTextForTesting {
  return [self->_allowButton.configuration.attributedTitle string];
}

@end
