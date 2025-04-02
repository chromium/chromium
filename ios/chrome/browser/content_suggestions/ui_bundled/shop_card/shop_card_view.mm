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
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/gradient_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "url/gurl.h"

namespace {
const CGFloat kHorizontalStackSpacing = 16.0f;
const CGFloat kVerticalStackSpacing = 6.0f;
const CGFloat kProductImageWidthHeight = 72.0;
const CGFloat kFaviconImageWidthHeight = 24.0;
const CGFloat kProductCornerRadius = 12.0;
const CGFloat kFaviconImageContainerTrailingMargin = -5.0;
const CGFloat kFaviconCornerRadius = 4.0;
const CGFloat kFaviconImageContainerTrailingCornerRadius = 8.0;

// Alpha for top of gradient overlay.
// TODO(crbug.com/392970898): add support for no product image case
const CGFloat kGradientOverlayTopAlpha = 0.0;

// Alpha for bottom of gradient overlay.
// TODO(crbug.com/392970898): add support for no product image case
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
  UIView* _faviconImageContainer;
  UIImageView* _productImage;
  UIImageView* _faviconImage;
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
  [self populateFaviconImageAndContainer:faviconImage];
  [self addFaviconToViewIfPresent];
  [self addFaviconImageAndContainerConstraintsIfPresent];
}

- (void)configureViewForTrackedProducts:(ShopCardItem*)configItem {
  _titleLabel = [[UILabel alloc] init];
  _titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  _titleLabel.numberOfLines = 1;
  _titleLabel.lineBreakMode = NSLineBreakByTruncatingTail;
  _titleLabel.font =
      CreateDynamicFont(UIFontTextStyleFootnote, UIFontWeightSemibold, self);
  _titleLabel.adjustsFontForContentSizeCategory = YES;
  _titleLabel.text = _item.shopCardData.productTitle;

  _urlLabel = [[UILabel alloc] init];
  _urlLabel.text = [self hostnameFromGURL:_item.shopCardData.productURL];
  _urlLabel.numberOfLines = 1;
  _urlLabel.lineBreakMode = NSLineBreakByTruncatingTail;
  _urlLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  _urlLabel.adjustsFontForContentSizeCategory = YES;
  _urlLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];

  _priceNotificationsChip = [[PriceNotificationsPriceChipView alloc] init];
  _priceNotificationsChip.previousPriceFont =
      CreateDynamicFont(UIFontTextStyleFootnote, UIFontWeightMedium);
  _priceNotificationsChip.currentPriceFont =
      CreateDynamicFont(UIFontTextStyleFootnote, UIFontWeightMedium);
  _priceNotificationsChip.strikeoutPreviousPrice = YES;
  [_priceNotificationsChip
       setPriceDrop:_item.shopCardData.priceDrop->current_price
      previousPrice:_item.shopCardData.priceDrop->previous_price];

  _productAndFaviconContainer = [[UIView alloc] init];
  _productImage = [[UIImageView alloc] init];
  UIImage* retrievedProductImage =
      [UIImage imageWithData:_item.shopCardData.productImage
                       scale:[UIScreen mainScreen].scale];

  _productImage.image = retrievedProductImage;
  _productImage.contentMode = UIViewContentModeScaleAspectFill;
  _productImage.translatesAutoresizingMaskIntoConstraints = NO;

  _productImage.layer.borderWidth = 0;
  _productImage.layer.cornerRadius = kProductCornerRadius;
  _productImage.layer.masksToBounds = YES;
  _productImage.backgroundColor = UIColor.whiteColor;

  _gradientOverlay = [[GradientView alloc]
      initWithTopColor:[[UIColor blackColor]
                           colorWithAlphaComponent:kGradientOverlayTopAlpha]
           bottomColor:[[UIColor blackColor] colorWithAlphaComponent:
                                                 kGradientOverlayBottomAlpha]];
  _gradientOverlay.layer.masksToBounds = YES;
  _gradientOverlay.translatesAutoresizingMaskIntoConstraints = NO;

  if (configItem.shopCardData && configItem.shopCardData.faviconImage) {
    [self
        populateFaviconImageAndContainer:configItem.shopCardData.faviconImage];
  }

  // Define hierarchy
  [_productAndFaviconContainer addSubview:_productImage];
  [_productImage addSubview:_gradientOverlay];

  [self addFaviconToViewIfPresent];

  // Add constraints after the hierarchy is defined
  [_productAndFaviconContainer bringSubviewToFront:_gradientOverlay];
  [self addConstraintsForProductImage];
  [NSLayoutConstraint activateConstraints:@[
    [_gradientOverlay.heightAnchor
        constraintEqualToConstant:kProductImageWidthHeight],
    [_gradientOverlay.widthAnchor
        constraintEqualToAnchor:_gradientOverlay.heightAnchor]
  ]];
  AddSameConstraints(_gradientOverlay, _productImage);
  AddSameConstraints(_productImage, _productAndFaviconContainer);

  [self addFaviconImageAndContainerConstraintsIfPresent];

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
  _contentStack.alignment = UIStackViewAlignmentTop;
  [self addSubview:_contentStack];
  AddSameConstraints(_contentStack, self);
}

// Returns the tab hostname from the given `URL`.
- (NSString*)hostnameFromGURL:(GURL)URL {
  return base::SysUTF16ToNSString(
      url_formatter::
          FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
              URL));
}

- (void)addConstraintsForProductImage {
  if (_productImage) {
    [NSLayoutConstraint activateConstraints:@[
      [_productImage.heightAnchor
          constraintEqualToConstant:kProductImageWidthHeight],
      [_productImage.widthAnchor
          constraintEqualToAnchor:_productImage.heightAnchor]
    ]];
  }
}

- (void)addConstraintsForFaviconImage {
  [NSLayoutConstraint activateConstraints:@[
    [_faviconImage.heightAnchor
        constraintEqualToConstant:kFaviconImageWidthHeight],
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

- (void)populateFaviconImageAndContainer:(UIImage*)faviconImage {
  _faviconImageContainer = [[UIView alloc] init];
  _faviconImage = [[UIImageView alloc] init];
  _faviconImage.image = faviconImage;
  _faviconImage.contentMode = UIViewContentModeScaleAspectFill;
  _faviconImage.translatesAutoresizingMaskIntoConstraints = NO;

  _faviconImage.layer.borderWidth = 0;
  _faviconImage.layer.masksToBounds = YES;
  _faviconImageContainer.layer.cornerRadius = kFaviconCornerRadius;
  _faviconImageContainer.layer.masksToBounds = YES;

  _faviconImageContainer.layer.mask =
      [self faviconMaskWithRadius:kFaviconImageContainerTrailingCornerRadius
                 imageHeightWidth:kFaviconImageWidthHeight];
}

- (void)addFaviconToViewIfPresent {
  if (_faviconImage) {
    [_productAndFaviconContainer addSubview:_faviconImageContainer];
    [_faviconImageContainer addSubview:_faviconImage];
  }
}

- (void)addFaviconImageAndContainerConstraintsIfPresent {
  if (_faviconImage) {
    [self addConstraintsForFaviconImage];
    AddSameConstraints(_faviconImage, _faviconImageContainer);
    // Place the favicon to trailing-bottom of product image
    _faviconImageContainer.translatesAutoresizingMaskIntoConstraints = NO;
    [self addConstraintsForFaviconContainerToTrailingEdge];
  }
}

@end
