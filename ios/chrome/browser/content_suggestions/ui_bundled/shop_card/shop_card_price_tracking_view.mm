// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/shop_card/shop_card_price_tracking_view.h"

#import "base/strings/sys_string_conversions.h"
#import "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/shop_card/shop_card_data.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/tab_resumption/tab_resumption_commands.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/tab_resumption/tab_resumption_item.h"
#import "ios/chrome/browser/price_notifications/ui_bundled/cells/price_notifications_price_chip_view.h"
#import "ios/chrome/browser/price_notifications/ui_bundled/cells/price_notifications_track_button.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {

const CGFloat kDefaultGlobeSymbolSize = 22.0;
const CGFloat kFaviconCornerRadius = 7.0;
const CGFloat kFaviconImageWidthHeight = 30.0;
const CGFloat kHorizontalStackSpacing = 16.0f;
const CGFloat kProductCornerRadius = 12.0;
const CGFloat kShopCardProductImageWidthHeight = 56.0;
const CGFloat kVerticalStackSpacing = 6.0f;

}  // namespace

@implementation ShopCardPriceTrackingView {
  // Item used to configure the view.
  TabResumptionItem* _item;

  UIStackView* _textStack;
  UILabel* _titleLabel;
  UILabel* _urlLabel;
  PriceNotificationsPriceChipView* _priceNotificationsChip;

  // Left side of ShopCard, holds product image with favicon.
  UIView* _productAndFaviconContainer;
  // Container, or grey if using default favicon globe image
  UIView* _faviconImageContainer;
  // The product image, or a placeholder gray if no product
  UIImageView* _productImage;
  // The favicon image, or a placeholder globe if no favicon
  UIImageView* _faviconImage;

  UIStackView* _contentStack;
  UIButton* _trackPriceButton;
}

- (instancetype)initWithItem:(TabResumptionItem*)item {
  self = [super init];
  if (self) {
    _item = item;
  }
  [self addTapGestureRecognizer];

  if (_item.contentImage) {
    // Case 1: Product image present.
    // Initialize + Styling
    [self addProductImage];

    // Hierarchy
    [_productAndFaviconContainer addSubview:_productImage];

    // Constraints
    [self addWidthConstraintsForProductImage:kShopCardProductImageWidthHeight];
    AddSameConstraints(_productImage, _productAndFaviconContainer);

  } else if (_item.faviconImage) {
    // Case 2: Only favicon image present.
    // Init and style the container and favicon
    [self addProductImageEmptyGray];
    [self addFaviconImageAndContainer:_item.faviconImage];
    _faviconImageContainer.backgroundColor = UIColor.whiteColor;

    // Hierarchy
    [_productAndFaviconContainer addSubview:_faviconImageContainer];
    [_productAndFaviconContainer addSubview:_productImage];
    [_faviconImageContainer addSubview:_faviconImage];
    [_productAndFaviconContainer bringSubviewToFront:_faviconImageContainer];

    // Constraints
    [self addWidthConstraintsForProductImage:kShopCardProductImageWidthHeight];
    AddSameConstraints(_productImage, _productAndFaviconContainer);
    [self addWidthConstraintsForFaviconImage:kFaviconImageWidthHeight];
    AddSameConstraints(_faviconImage, _faviconImageContainer);
    AddSameCenterConstraints(_productImage, _faviconImageContainer);

  } else {
    // Case 3: No favicon or product image.
    // Init and style
    [self addProductImageEmptyGray];
    [self addFaviconImageAndContainer:[self makeDefaultFaviconUIImage]];
    [self addFaviconImageContainerColorForGlobe];

    // Hierarchy
    [_productAndFaviconContainer addSubview:_faviconImageContainer];
    [_productAndFaviconContainer addSubview:_productImage];
    [_faviconImageContainer addSubview:_faviconImage];
    [_productAndFaviconContainer bringSubviewToFront:_faviconImageContainer];

    // Constraints
    // The globe symbol is smaller than the favicon container overall.
    [NSLayoutConstraint activateConstraints:@[
      [_faviconImageContainer.widthAnchor
          constraintEqualToConstant:kFaviconImageWidthHeight],
      [_faviconImageContainer.heightAnchor
          constraintEqualToAnchor:_faviconImageContainer.widthAnchor],
    ]];
    [self addWidthConstraintsForFaviconImage:kDefaultGlobeSymbolSize];
    [self addWidthConstraintsForProductImage:kShopCardProductImageWidthHeight];
    AddSameConstraints(_productImage, _productAndFaviconContainer);
    AddSameCenterConstraints(_faviconImage, _faviconImageContainer);
    AddSameCenterConstraints(_productImage, _faviconImageContainer);
  }

  // Populate the rest of the card: the text+price chip, and the track button.
  _trackPriceButton =
      [[PriceNotificationsTrackButton alloc] initWithLightVariant:YES];
  [_trackPriceButton addTarget:self
                        action:@selector(trackItem)
              forControlEvents:UIControlEventTouchUpInside];

  [self populateTitleLabel];
  [self populateUrlLabel];
  [self populatePriceNotificationChip];
  _textStack = [[UIStackView alloc] initWithArrangedSubviews:@[
    _titleLabel, _urlLabel, _priceNotificationsChip
  ]];
  _textStack.axis = UILayoutConstraintAxisVertical;
  _textStack.alignment = UIStackViewAlignmentLeading;
  _textStack.spacing = kVerticalStackSpacing;

  _contentStack = [[UIStackView alloc] initWithArrangedSubviews:@[
    _productAndFaviconContainer, _textStack, _trackPriceButton
  ]];
  _contentStack.translatesAutoresizingMaskIntoConstraints = NO;
  _contentStack.spacing = kHorizontalStackSpacing;
  _contentStack.alignment = UIStackViewAlignmentCenter;
  _contentStack.axis = UILayoutConstraintAxisHorizontal;
  _contentStack.distribution = UIStackViewDistributionFillProportionally;

  [self addSubview:_contentStack];
  AddSameConstraints(_contentStack, self);

  // Accessibility
  _contentStack.accessibilityElements = @[ _textStack, _trackPriceButton ];
  self.isAccessibilityElement = NO;
  _textStack.accessibilityLabel = _item.shopCardData.accessibilityString;
  _textStack.isAccessibilityElement = YES;
  _textStack.accessibilityTraits = UIAccessibilityTraitButton;
  _priceNotificationsChip.isAccessibilityElement = YES;
  _trackPriceButton.isAccessibilityElement = YES;
  _trackPriceButton.accessibilityTraits = UIAccessibilityTraitButton;
  NSString* trackPriceButtonAccessibilityLabel = [NSString
      stringWithFormat:
          @"%@ %@",
          l10n_util::GetNSString(
              IDS_IOS_CONTENT_SUGGESTIONS_SHOPCARD_TRACK_PRICE_BUTTON),
          _item.tabTitle];
  _trackPriceButton.accessibilityLabel = trackPriceButtonAccessibilityLabel;
  _titleLabel.accessibilityTraits |= UIAccessibilityTraitHeader;
  // For larger font size, domain
  if (@available(iOS 17, *)) {
    NSArray<UITrait>* traits = TraitCollectionSetForTraits(
        @[ UITraitPreferredContentSizeCategory.class ]);
    [self registerForTraitChanges:traits
                       withAction:@selector(hideDomainOnTraitChange)];
  }
  return self;
}

- (void)hideDomainOnTraitChange {
  _urlLabel.hidden = self.traitCollection.preferredContentSizeCategory >
                     UIContentSizeCategoryExtraExtraLarge;
}

- (UIImage*)makeDefaultFaviconUIImage {
  return DefaultSymbolWithPointSize(kGlobeAmericasSymbol,
                                    kDefaultGlobeSymbolSize);
}

- (void)addFaviconImageContainerColorForGlobe {
  _faviconImageContainer.backgroundColor = [UIColor colorNamed:kBlue500Color];
  // Color inside the globe icon
  _faviconImageContainer.tintColor = UIColor.whiteColor;
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

- (void)addProductImageEmptyGray {
  _productAndFaviconContainer = [[UIView alloc] init];
  _productImage = [[UIImageView alloc] init];
  _productImage.backgroundColor = [UIColor colorNamed:kTertiaryBackgroundColor];
  _productImage.contentMode = UIViewContentModeScaleAspectFill;
  _productImage.translatesAutoresizingMaskIntoConstraints = NO;
  _productImage.layer.borderWidth = 0;
  _productImage.layer.cornerRadius = kProductCornerRadius;
  _productImage.layer.masksToBounds = YES;
}

- (void)addWidthConstraintsForFaviconImage:(const CGFloat)faviconWidth {
  [NSLayoutConstraint activateConstraints:@[
    [_faviconImage.heightAnchor constraintEqualToConstant:faviconWidth],
    [_faviconImage.widthAnchor
        constraintEqualToAnchor:_faviconImage.heightAnchor]
  ]];
}

- (void)addProductImage {
  _productAndFaviconContainer = [[UIView alloc] init];
  _productImage = [[UIImageView alloc] init];

  _productImage.image = _item.contentImage;
  _productImage.backgroundColor = UIColor.whiteColor;

  _productImage.contentMode = UIViewContentModeScaleAspectFill;
  _productImage.translatesAutoresizingMaskIntoConstraints = NO;
  _productImage.layer.borderWidth = 0;
  _productImage.layer.cornerRadius = kProductCornerRadius;
  _productImage.layer.masksToBounds = YES;
}

- (void)addWidthConstraintsForProductImage:(const CGFloat)width {
  [NSLayoutConstraint activateConstraints:@[
    [_productImage.heightAnchor constraintEqualToConstant:width],
    [_productImage.widthAnchor
        constraintEqualToAnchor:_productImage.heightAnchor]
  ]];
}

- (void)populatePriceNotificationChip {
  _priceNotificationsChip = [[PriceNotificationsPriceChipView alloc] init];
  _priceNotificationsChip.currentPriceFont =
      PreferredFontForTextStyle(UIFontTextStyleFootnote, UIFontWeightMedium);
  // When there is no price drop, the previous price is the current price
  [_priceNotificationsChip setPriceDrop:nil
                          previousPrice:_item.shopCardData.currentPrice];
}

- (void)populateTitleLabel {
  _titleLabel = [[UILabel alloc] init];
  _titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  _titleLabel.numberOfLines = 1;
  _titleLabel.lineBreakMode = NSLineBreakByTruncatingTail;
  _titleLabel.font =
      PreferredFontForTextStyle(UIFontTextStyleFootnote, UIFontWeightSemibold);
  _titleLabel.adjustsFontForContentSizeCategory = YES;
  NSString* text = [_item.tabTitle length]
                       ? _item.tabTitle
                       : l10n_util::GetNSString(
                             IDS_IOS_TAB_RESUMPTION_TAB_TITLE_PLACEHOLDER);
  _titleLabel.text = text;
}

- (void)populateUrlLabel {
  // TODO(crbug.com/410526534): confirm text behavior for local vs remote.
  _urlLabel = [[UILabel alloc] init];
  _urlLabel.text = [self configuredHostNameAndSessionLabel];
  _urlLabel.numberOfLines = 1;
  _urlLabel.lineBreakMode = NSLineBreakByTruncatingTail;
  _urlLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  _urlLabel.adjustsFontForContentSizeCategory = YES;
  _urlLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
}

// Configures and returns the UILabel that contains the session name.
- (NSString*)configuredHostNameAndSessionLabel {
  if (_item.itemType == kLastSyncedTab && _item.sessionName) {
    return [NSString stringWithFormat:@"%@ • %@",
                                      [self hostnameFromGURL:_item.tabURL],
                                      _item.sessionName];
  } else {
    return
        [NSString stringWithFormat:@"%@", [self hostnameFromGURL:_item.tabURL]];
  }
}

// Initiates price tracking.
- (void)trackItem {
  [self.commandHandler trackShopCardItem:_item];
}

// Returns the tab hostname from the given `URL`.
- (NSString*)hostnameFromGURL:(GURL)URL {
  return base::SysUTF16ToNSString(
      url_formatter::
          FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
              URL));
}

- (void)addTapGestureRecognizer {
  UITapGestureRecognizer* tapRecognizer = [[UITapGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(shopCardItemTapped:)];
  [self addGestureRecognizer:tapRecognizer];
}

- (void)shopCardItemTapped:(UIGestureRecognizer*)sender {
  [self.commandHandler openTabResumptionItem:_item];
}

@end
