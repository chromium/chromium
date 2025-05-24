// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/shop_card/shop_card_view.h"

#import "base/i18n/rtl.h"
#import "base/strings/sys_string_conversions.h"
#import "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/shop_card/shop_card_commands.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/shop_card/shop_card_data.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/shop_card/shop_card_favicon_consumer.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/shop_card/shop_card_item.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/shop_card/shop_card_mediator.h"
#import "ios/chrome/browser/price_notifications/ui_bundled/cells/price_notifications_price_chip_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/gradient_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "url/gurl.h"

namespace {
NSString* const kShopCardViewIdentifier = @"kShopCardViewIdentifier";
const CGFloat kHorizontalStackSpacing = 16.0f;
const CGFloat kVerticalStackSpacing = 6.0f;
const CGFloat kCenterSymbolSize = 20.0;
const CGFloat kShopCardProductImageWidthHeight = 72.0;
const CGFloat kProductImageEmptyWidthHeight = 56.0;
const CGFloat kFaviconImageWidthHeight = 24.0;
const CGFloat kFaviconDefaultImageWidthHeight = 22.0;
const CGFloat kFaviconCenterImageWidthHeight = 30.0;
const CGFloat kProductCornerRadius = 12.0;
const CGFloat kFaviconImageContainerTrailingMargin = -5.0;
const CGFloat kFaviconCornerRadius = 4.0;
const CGFloat kFaviconImageContainerTrailingCornerRadius = 8.0;

const CGFloat kFaviconContainerShadowVerticalOffset = 4.0;
const CGFloat kFaviconShadowOpacity = 0.12;
const CGFloat kFaviconShadowRadius = 11.0;

// Alpha for top and bottom of gradient overlay.
const CGFloat kGradientOverlayTopAlpha = 0.0;
const CGFloat kGradientOverlayBottomAlpha = 0.14;

}  // namespace

@interface ShopCardModuleView () <ShopCardFaviconConsumer>
@end

@implementation ShopCardModuleView {
  ShopCardItem* _item;
  // Holds content of ShopCard. includes productAndFavicon on the left,
  // and textStack on the right.
  UIStackView* _contentStack;

  // Right side of ShopCard, holds text title, url, and price drop.
  UIStackView* _textStack;
  UILabel* _titleLabel;
  UILabel* _urlLabel;

  // Left side of ShopCard, holds product image with favicon.
  UIView* _productAndFaviconContainer;
  UIView* _faviconImageContainer;  // container, or grey if using default
                                   // favicon globe image
  UIImageView*
      _productImage;  // the product image, or a placeholder gray if no product
  UIImageView*
      _faviconImage;  // the favicon image, or a placeholder globe if no favicon
  UIView* _gradientOverlay;

  PriceNotificationsPriceChipView* _priceNotificationsChip;
}

- (instancetype)initWithFrame {
  self = [super initWithFrame:CGRectZero];
  return self;
}

- (void)configureView:(ShopCardItem*)config {
  if (!config) {
    return;
  }
  [self addTapGestureRecognizer];
  _item = config;

  if (config.shopCardData.shopCardItemType ==
      ShopCardItemType::kPriceDropForTrackedProducts) {
    [self configureViewForTrackedProducts:_item];
  } else if (config.shopCardData.shopCardItemType ==
             ShopCardItemType::kReviews) {
    // TODO: crbug.com/394638800 - render correct view when data available
  }
}

#pragma mark - ShopCardFaviconConsumer
- (void)faviconCompleted:(UIImage*)faviconImage {
  _item.shopCardData.faviconImage = faviconImage;
  [self configureViewForTrackedProducts:_item];
}

- (void)configureViewForTrackedProducts:(ShopCardItem*)configItem {
  [self populateTitleLabel];
  [self populateUrlLabel];
  [self populatePriceNotificationChip];

  // Case 1: both are present, favicon on bottom right.
  if (_item.shopCardData.productImage && _item.shopCardData.faviconImage) {
    // Styling
    [self addProductImageAndOverlay];
    [self addFaviconImageAndContainer:_item.shopCardData.faviconImage];
    _faviconImageContainer.backgroundColor = UIColor.whiteColor;
    _faviconImageContainer.layer.mask =
        [self faviconMaskWithRadius:kFaviconImageContainerTrailingCornerRadius
                   imageHeightWidth:kFaviconImageWidthHeight];

    // Hierarchy
    [_productAndFaviconContainer addSubview:_productImage];
    [_productImage addSubview:_gradientOverlay];
    [_productAndFaviconContainer addSubview:_faviconImageContainer];
    [_faviconImageContainer addSubview:_faviconImage];

    // Constraints
    [self addWidthConstraintsForProductImage:kShopCardProductImageWidthHeight];
    [_productAndFaviconContainer bringSubviewToFront:_gradientOverlay];
    AddSameConstraints(_gradientOverlay, _productImage);
    AddSameConstraints(_productImage, _productAndFaviconContainer);
    [self addWidthConstraintsForFaviconImage:kFaviconImageWidthHeight];
    AddSameConstraints(_faviconImage, _faviconImageContainer);
    [self addConstraintsForFaviconContainerToTrailingEdge];
  }

  // Case 2: product image and overlay only, favicon absent.
  if (_item.shopCardData.productImage && !_item.shopCardData.faviconImage) {
    // Styling
    [self addProductImageAndOverlay];

    // Hierarchy
    [_productAndFaviconContainer addSubview:_productImage];
    [_productImage addSubview:_gradientOverlay];

    // Constraints
    [self addWidthConstraintsForProductImage:kShopCardProductImageWidthHeight];
    [self addGradientOverlayConstraints];
    [_productAndFaviconContainer bringSubviewToFront:_gradientOverlay];
    AddSameConstraints(_gradientOverlay, _productImage);
    AddSameConstraints(_productImage, _productAndFaviconContainer);
  }

  // Case 3: no product only favicon in the center. product image is empty -
  // grey.
  if (!_item.shopCardData.productImage && _item.shopCardData.faviconImage) {
    // Styling
    [self addProductImageEmptyGray];
    [self addFaviconImageAndContainer:_item.shopCardData.faviconImage];
    _faviconImageContainer.backgroundColor = UIColor.whiteColor;
    [self addShadowForFaviconContainer];

    // Hierarchy
    [_productAndFaviconContainer addSubview:_productImage];
    [_productAndFaviconContainer addSubview:_faviconImageContainer];
    [_faviconImageContainer addSubview:_faviconImage];

    // Constraints
    [self addWidthConstraintsForProductImage:kProductImageEmptyWidthHeight];
    [self addWidthConstraintsForFaviconImage:kFaviconCenterImageWidthHeight];
    AddSameConstraints(_productImage, _productAndFaviconContainer);
    AddSameConstraints(_faviconImage, _faviconImageContainer);
    [self addConstraintsForFaviconContainerToProductContainerCenter];
  }

  // Case 4: no favicon or product. globe in the center, product image is grey.
  if (!_item.shopCardData.productImage && !_item.shopCardData.faviconImage) {
    // Styling
    [self addProductImageEmptyGray];
    [self addFaviconImageAndContainer:[self makeDefaultFaviconUIImage]];
    [self addFaviconImageContainerColorForGlobe];
    [self addShadowForFaviconContainer];

    // Hierarchy
    [_productAndFaviconContainer addSubview:_productImage];
    [_productAndFaviconContainer addSubview:_faviconImageContainer];
    [_faviconImageContainer addSubview:_faviconImage];

    // Constraints
    [self addWidthConstraintsForProductImage:kProductImageEmptyWidthHeight];
    [self addWidthConstraintsForFaviconImage:kFaviconDefaultImageWidthHeight];
    [self addWidthConstraintsForFaviconContainerDefaultGlobe];
    AddSameConstraints(_productImage, _productAndFaviconContainer);
    [self addConstraintsForFaviconImageToFaviconImageContainerCenter];
    [self addConstraintsForFaviconContainerToProductContainerCenter];
  }

  // Lay out text stack
  _textStack = [[UIStackView alloc] initWithArrangedSubviews:@[
    _titleLabel, _urlLabel, _priceNotificationsChip
  ]];
  _textStack.axis = UILayoutConstraintAxisVertical;
  _textStack.alignment = UIStackViewAlignmentLeading;
  _textStack.spacing = kVerticalStackSpacing;

  _contentStack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ _productAndFaviconContainer, _textStack ]];
  _contentStack.translatesAutoresizingMaskIntoConstraints = NO;
  _contentStack.spacing = kHorizontalStackSpacing;
  _contentStack.alignment = UIStackViewAlignmentCenter;
  [self addSubview:_contentStack];
  AddSameConstraints(_contentStack, self);

  // Accessibility
  self.isAccessibilityElement = YES;
  self.accessibilityIdentifier = kShopCardViewIdentifier;
  self.accessibilityTraits = UIAccessibilityTraitButton;
  self.accessibilityLabel = _item.shopCardData.accessibilityString;
  _priceNotificationsChip.isAccessibilityElement = YES;
  _titleLabel.accessibilityTraits |= UIAccessibilityTraitHeader;
  // For larger font size, hide price chip.
  if (@available(iOS 17, *)) {
    NSArray<UITrait>* traits = TraitCollectionSetForTraits(
        @[ UITraitPreferredContentSizeCategory.class ]);
    [self registerForTraitChanges:traits
                       withAction:@selector(hideDomainOnTraitChange)];
  }
}

- (void)hideDomainOnTraitChange {
  _urlLabel.hidden = self.traitCollection.preferredContentSizeCategory >
                     UIContentSizeCategoryExtraExtraLarge;
}

// Returns the tab hostname from the given `URL`.
- (NSString*)hostnameFromGURL:(GURL)URL {
  return base::SysUTF16ToNSString(
      url_formatter::
          FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
              URL));
}

- (void)populateTitleLabel {
  _titleLabel = [[UILabel alloc] init];
  _titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  _titleLabel.numberOfLines = 1;
  _titleLabel.lineBreakMode = NSLineBreakByTruncatingTail;
  _titleLabel.font =
      PreferredFontForTextStyle(UIFontTextStyleFootnote, UIFontWeightSemibold);
  _titleLabel.adjustsFontForContentSizeCategory = YES;
  _titleLabel.text = _item.shopCardData.productTitle;
}

- (void)populateUrlLabel {
  _urlLabel = [[UILabel alloc] init];
  _urlLabel.text = [self hostnameFromGURL:_item.shopCardData.productURL];
  _urlLabel.numberOfLines = 1;
  _urlLabel.lineBreakMode = NSLineBreakByTruncatingTail;
  _urlLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  _urlLabel.adjustsFontForContentSizeCategory = YES;
  _urlLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
}

- (void)populatePriceNotificationChip {
  _priceNotificationsChip = [[PriceNotificationsPriceChipView alloc] init];
  _priceNotificationsChip.previousPriceFont =
      PreferredFontForTextStyle(UIFontTextStyleFootnote, UIFontWeightMedium);
  _priceNotificationsChip.currentPriceFont =
      PreferredFontForTextStyle(UIFontTextStyleFootnote, UIFontWeightMedium);
  _priceNotificationsChip.strikeoutPreviousPrice = YES;
  [_priceNotificationsChip
       setPriceDrop:_item.shopCardData.priceDrop->current_price
      previousPrice:_item.shopCardData.priceDrop->previous_price];
}

- (void)addWidthConstraintsForProductImage:(const CGFloat)width {
  [NSLayoutConstraint activateConstraints:@[
    [_productImage.heightAnchor constraintEqualToConstant:width],
    [_productImage.widthAnchor
        constraintEqualToAnchor:_productImage.heightAnchor]
  ]];
}

- (void)addWidthConstraintsForFaviconImage:(const CGFloat)faviconWidth {
  [NSLayoutConstraint activateConstraints:@[
    [_faviconImage.heightAnchor constraintEqualToConstant:faviconWidth],
    [_faviconImage.widthAnchor
        constraintEqualToAnchor:_faviconImage.heightAnchor]
  ]];
}

- (void)addConstraintsForFaviconContainerToTrailingEdge {
  [NSLayoutConstraint activateConstraints:@[
    [_faviconImageContainer.trailingAnchor
        constraintEqualToAnchor:_productAndFaviconContainer.trailingAnchor
                       constant:kFaviconImageContainerTrailingMargin],
    [_faviconImageContainer.bottomAnchor
        constraintEqualToAnchor:_productAndFaviconContainer.bottomAnchor
                       constant:kFaviconImageContainerTrailingMargin]
  ]];
}

- (void)addConstraintsForFaviconContainerToProductContainerCenter {
  [NSLayoutConstraint activateConstraints:@[
    [_faviconImageContainer.centerXAnchor
        constraintEqualToAnchor:_productAndFaviconContainer.centerXAnchor],
    [_faviconImageContainer.centerYAnchor
        constraintEqualToAnchor:_productAndFaviconContainer.centerYAnchor]
  ]];
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

- (void)addTapGestureRecognizer {
  UITapGestureRecognizer* tapRecognizer = [[UITapGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(shopCardItemTapped:)];
  [self addGestureRecognizer:tapRecognizer];
}

- (void)shopCardItemTapped:(UIGestureRecognizer*)sender {
  [self.commandHandler openShopCardItem:_item];
}

- (UIImage*)makeDefaultFaviconUIImage {
  return DefaultSymbolWithPointSize(kGlobeAmericasSymbol, kCenterSymbolSize);
}

- (void)addFaviconImageAndContainer:(UIImage*)faviconImage {
  _faviconImageContainer = [[UIView alloc] init];
  _faviconImage = [[UIImageView alloc] init];
  _faviconImage.image = faviconImage;
  _faviconImage.contentMode = UIViewContentModeScaleAspectFill;
  _faviconImage.translatesAutoresizingMaskIntoConstraints = NO;

  _faviconImage.layer.borderWidth = 0;
  _faviconImage.layer.masksToBounds = YES;
  _faviconImageContainer.layer.cornerRadius = kFaviconCornerRadius;
  _faviconImageContainer.layer.masksToBounds = YES;
  _faviconImageContainer.translatesAutoresizingMaskIntoConstraints = NO;
}

- (void)addShadowForFaviconContainer {
  _faviconImageContainer.layer.shadowColor = [UIColor blackColor].CGColor;
  _faviconImageContainer.layer.shadowOffset =
      CGSizeMake(0, kFaviconContainerShadowVerticalOffset /*4*/);
  _faviconImageContainer.layer.shadowOpacity = kFaviconShadowOpacity;  // 12%
  _faviconImageContainer.layer.shadowRadius = kFaviconShadowRadius;    // 11
}

- (void)addFaviconImageContainerColorForGlobe {
  _faviconImageContainer.backgroundColor = [UIColor colorNamed:kBlue500Color];
  // Color inside the globe icon
  _faviconImageContainer.tintColor = UIColor.whiteColor;
}

- (void)addConstraintsForFaviconImageToFaviconImageContainerCenter {
  [NSLayoutConstraint activateConstraints:@[
    [_faviconImage.centerXAnchor
        constraintEqualToAnchor:_faviconImageContainer.centerXAnchor],
    [_faviconImage.centerYAnchor
        constraintEqualToAnchor:_faviconImageContainer.centerYAnchor],
  ]];
}

- (void)addWidthConstraintsForFaviconContainerDefaultGlobe {
  [NSLayoutConstraint activateConstraints:@[
    [_faviconImageContainer.widthAnchor
        constraintEqualToConstant:kFaviconCenterImageWidthHeight],
    [_faviconImageContainer.heightAnchor
        constraintEqualToAnchor:_faviconImageContainer.widthAnchor],
  ]];
}

- (void)addProductImageEmptyGray {
  _productAndFaviconContainer = [[UIView alloc] init];
  _productImage = [[UIImageView alloc] init];
  _productImage.backgroundColor =
      [UIColor colorNamed:kTertiaryBackgroundColor];
  _productImage.contentMode = UIViewContentModeScaleAspectFill;
  _productImage.translatesAutoresizingMaskIntoConstraints = NO;
  _productImage.layer.borderWidth = 0;
  _productImage.layer.cornerRadius = kProductCornerRadius;
  _productImage.layer.masksToBounds = YES;
}

- (void)addProductImageAndOverlay {
  _productAndFaviconContainer = [[UIView alloc] init];
  _productImage = [[UIImageView alloc] init];

  UIImage* retrievedProductImage =
      [UIImage imageWithData:_item.shopCardData.productImage
                       scale:[UIScreen mainScreen].scale];
  _productImage.image = retrievedProductImage;
  _productImage.backgroundColor = UIColor.whiteColor;
  _gradientOverlay = [[GradientView alloc]
      initWithTopColor:[[UIColor blackColor]
                           colorWithAlphaComponent:kGradientOverlayTopAlpha]
           bottomColor:[[UIColor blackColor] colorWithAlphaComponent:
                                                 kGradientOverlayBottomAlpha]];
  _gradientOverlay.layer.masksToBounds = YES;
  _gradientOverlay.layer.zPosition = 1;
  _gradientOverlay.translatesAutoresizingMaskIntoConstraints = NO;

  _productImage.contentMode = UIViewContentModeScaleAspectFill;
  _productImage.translatesAutoresizingMaskIntoConstraints = NO;
  _productImage.layer.borderWidth = 0;
  _productImage.layer.cornerRadius = kProductCornerRadius;
  _productImage.layer.masksToBounds = YES;
}

- (void)addGradientOverlayConstraints {
  [NSLayoutConstraint activateConstraints:@[
    [_gradientOverlay.heightAnchor
        constraintEqualToConstant:kShopCardProductImageWidthHeight],
    [_gradientOverlay.widthAnchor
        constraintEqualToAnchor:_gradientOverlay.heightAnchor]
  ]];
}

@end
