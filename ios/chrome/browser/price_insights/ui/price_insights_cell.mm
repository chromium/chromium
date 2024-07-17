// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/price_insights/ui/price_insights_cell.h"

#import "base/strings/sys_string_conversions.h"
#import "components/commerce/core/commerce_constants.h"
#import "components/payments/core/currency_formatter.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/price_insights/ui/price_history_swift.h"
#import "ios/chrome/browser/price_insights/ui/price_insights_constants.h"
#import "ios/chrome/browser/price_insights/ui/price_insights_mutator.h"
#import "ios/chrome/browser/price_insights/ui/price_ranger_slider.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

namespace {

// The horizontal inset for the content within the contentStackView.
const CGFloat kContentHorizontalInset = 20.0f;

// The vertical inset for the content within the contentStackView.
const CGFloat kContentVerticalInset = 16.0f;

// The horizontal inset between contentStackView and contentView.
const CGFloat kHorizontalInset = 16.0f;

// The spacing between the content stack views.
const CGFloat kContentStackViewSpacing = 4.0f;

// The spacing between price tracking vertical stack views.
const CGFloat kPriceTrackingVerticalStackViewSpacing = 2.0f;

// The spacing between price tracking stack views.
const CGFloat kHorizontalStackViewSpacing = 20.0f;

// Size of the icon.
const CGFloat kIconSize = 20.0f;

// Size of the space between the graph and the text in Price History.
const CGFloat kPriceHistoryContentSpacing = 12.0f;

// Size of the space between the graph and the text in Price Range.
const CGFloat kPriceRangeContentSpacing = 16.0f;

// Height of Price History graph.
const CGFloat kPriceHistoryGraphHeight = 186.0f;

// The corner radius of this container.
const float kCornerRadius = 24;

// The horizontal padding for the track button.
const CGFloat kTrackButtonHorizontalPadding = 14.0f;

// The vertical padding for the track button.
const CGFloat kTrackButtonVerticalPadding = 3.0f;

// Formats a price amount in micro-units into a localized string representation
// for display.
std::u16string getFormattedCurrentPrice(int64_t amount_micro,
                                        std::string currency_code,
                                        std::string country_code) {
  float price = static_cast<float>(amount_micro) /
                static_cast<float>(commerce::kToMicroCurrency);
  payments::CurrencyFormatter formatter(currency_code, country_code);
  formatter.SetMaxFractionalDigits(2);
  return formatter.Format(base::NumberToString(price));
}

}  // namespace

@interface PriceInsightsCell ()

// Object with data related to price insights.
@property(nonatomic, strong) PriceInsightsItem* item;

@end

@implementation PriceInsightsCell {
  UIStackView* _priceTrackingStackView;
  UIStackView* _buyingOptionsStackView;
  UIStackView* _contentStackView;
  UIStackView* _priceHistoryStackView;
  UIStackView* _priceRangeStackView;
  UIButton* _trackButton;
  NSLayoutConstraint* _trackButtonWidthConstraint;
}

#pragma mark - Public

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _contentStackView = [[UIStackView alloc] init];
    _contentStackView.translatesAutoresizingMaskIntoConstraints = NO;
    _contentStackView.axis = UILayoutConstraintAxisVertical;
    _contentStackView.spacing = kContentStackViewSpacing;
    _contentStackView.distribution = UIStackViewDistributionFill;
    _contentStackView.alignment = UIStackViewAlignmentFill;
    _contentStackView.clipsToBounds = YES;
    _contentStackView.layer.cornerRadius = kCornerRadius;
    [_contentStackView setAccessibilityIdentifier:kContentStackViewIdentifier];

    [self.contentView addSubview:_contentStackView];
    AddSameConstraintsWithInsets(
        _contentStackView, self.contentView,
        NSDirectionalEdgeInsetsMake(0, kHorizontalInset, 0, kHorizontalInset));
  }
  return self;
}

- (void)configureWithItem:(PriceInsightsItem*)item {
  self.item = item;

  // Configure Price Trancking and Price Range.
  if (self.item.canPriceTrack ||
      ([self hasPriceRange] && [self hasPriceHistory])) {
    [self configurePriceTrackingAndRange];
    [_contentStackView addArrangedSubview:_priceTrackingStackView];
  }

  // Configure Price History.
  if ([self hasPriceHistory] && !self.item.currency.empty()) {
    NSString* title;
    NSString* primarySubtitle;
    NSString* secondarySubtitle;

    bool hasPriceTrackOrPriceRange =
        self.item.canPriceTrack || [self hasPriceRange];
    title = hasPriceTrackOrPriceRange
                ? l10n_util::GetNSString(IDS_PRICE_HISTORY_TITLE_SINGLE_OPTION)
                : self.item.title;
    NSString* priceHistoryDescription =
        hasPriceTrackOrPriceRange
            ? nil
            : l10n_util::GetNSString(IDS_PRICE_HISTORY_TITLE_SINGLE_OPTION);

    if ([self hasVariants]) {
      primarySubtitle = self.item.variants;
      secondarySubtitle = priceHistoryDescription;
    } else {
      primarySubtitle = priceHistoryDescription;
      secondarySubtitle = nil;
    }

    [self configurePriceHistoryWithTitle:title
                         primarySubtitle:primarySubtitle
                       secondarySubtitle:secondarySubtitle];

    [_contentStackView addArrangedSubview:_priceHistoryStackView];
  }

  // Configure Price Range
  if ([self hasPriceRange] && ![self hasPriceHistory]) {
    NSString* variantTitle =
        [self hasVariants]
            ? l10n_util::GetNSString(
                  IDS_PRICE_INSIGHTS_PRICE_RANGE_TITLE_VARIANT)
            : l10n_util::GetNSString(
                  IDS_PRICE_INSIGHTS_PRICE_RANGE_TITLE_NO_VARIANT);
    NSString* title = self.item.canPriceTrack ? variantTitle : self.item.title;
    NSString* subtitle = self.item.canPriceTrack ? nil : variantTitle;
    [self configurePriceRangeWithTitle:title subtitle:subtitle];
    [_contentStackView addArrangedSubview:_priceRangeStackView];
  }

  // Configure Buying options.
  if ([self hasPriceHistory] && [self hasPriceRange] &&
      self.item.buyingOptionsURL.is_valid()) {
    [self configureBuyingOptions];
    [_contentStackView addArrangedSubview:_buyingOptionsStackView];
  }
}

- (void)updateTrackButton:(BOOL)isTracking {
  self.item.isPriceTracked = isTracking;
  [self setOrUpdateTrackButton];
}

- (void)prepareForReuse {
  [super prepareForReuse];
  for (UIView* view in _contentStackView.arrangedSubviews) {
    [_contentStackView removeArrangedSubview:view];
    [view removeFromSuperview];
  }
}

#pragma mark - Private

// Returns whether or not price range is available.
- (BOOL)hasPriceRange {
  return self.item.lowPrice > 0 && self.item.highPrice > 0;
}

// Returns whether or not price has one typical price. If there is only one
// typical price, the high and low price are equal.
- (BOOL)hasPriceOneTypicalPrice {
  return [self hasPriceRange] ? self.item.highPrice == self.item.lowPrice : NO;
}

// Returns whether or not there are any variants.
- (BOOL)hasVariants {
  return self.item.variants.length > 0;
}

// Returns whether or not price history is available.
- (BOOL)hasPriceHistory {
  return self.item.priceHistory && [self.item.priceHistory count] > 0;
}

// Method that creates a view for both price tracking and price range, or solely
// for price tracking or price range when price history is also available.
- (void)configurePriceTrackingAndRange {
  UILabel* priceTrackingTitle = [self createLabel];
  [priceTrackingTitle setAccessibilityIdentifier:kPriceTrackingTitleIdentifier];
  priceTrackingTitle.font =
      CreateDynamicFont(UIFontTextStyleSubheadline, UIFontWeightSemibold);
  priceTrackingTitle.textColor = [UIColor colorNamed:kTextPrimaryColor];
  priceTrackingTitle.text = self.item.title;
  priceTrackingTitle.accessibilityTraits = UIAccessibilityTraitHeader;

  UILabel* priceTrackingSubtitle = [self createLabel];
  [priceTrackingSubtitle
      setAccessibilityIdentifier:kPriceTrackingSubtitleIdentifier];
  priceTrackingSubtitle.font =
      CreateDynamicFont(UIFontTextStyleSubheadline, UIFontWeightRegular);
  priceTrackingSubtitle.textColor = [UIColor colorNamed:kTextSecondaryColor];
  if ([self hasPriceRange] && [self hasPriceHistory]) {
    std::u16string lowPrice = getFormattedCurrentPrice(
        self.item.lowPrice, self.item.currency, self.item.country);
    std::u16string highPrice = getFormattedCurrentPrice(
        self.item.highPrice, self.item.currency, self.item.country);

    priceTrackingSubtitle.numberOfLines = 2;
    priceTrackingSubtitle.text =
        [self hasVariants]
            ? ([self hasPriceOneTypicalPrice]
                   ? l10n_util::GetNSStringF(
                         IDS_PRICE_RANGE_ALL_OPTIONS_ONE_TYPICAL_PRICE,
                         lowPrice)
                   : l10n_util::GetNSStringF(IDS_PRICE_RANGE_ALL_OPTIONS,
                                             lowPrice, highPrice))
            : ([self hasPriceOneTypicalPrice]
                   ? l10n_util::GetNSStringF(
                         IDS_PRICE_RANGE_SINGLE_OPTION_ONE_TYPICAL_PRICE,
                         lowPrice)
                   : l10n_util::GetNSStringF(IDS_PRICE_RANGE_SINGLE_OPTION,
                                             lowPrice, highPrice));
  } else {
    priceTrackingSubtitle.numberOfLines = 2;
    priceTrackingSubtitle.text =
        l10n_util::GetNSString(IDS_PRICE_TRACKING_DESCRIPTION);
  }

  UIStackView* verticalStack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ priceTrackingTitle, priceTrackingSubtitle ]];
  verticalStack.axis = UILayoutConstraintAxisVertical;
  verticalStack.distribution = UIStackViewDistributionFill;
  verticalStack.alignment = UIStackViewAlignmentLeading;
  verticalStack.spacing = kPriceTrackingVerticalStackViewSpacing;

  _priceTrackingStackView = [[UIStackView alloc] init];
  [_priceTrackingStackView
      setAccessibilityIdentifier:kPriceTrackingStackViewIdentifier];
  [_priceTrackingStackView addArrangedSubview:verticalStack];

  if (self.item.canPriceTrack) {
    [self setOrUpdateTrackButton];
    [_trackButton setAccessibilityIdentifier:kPriceTrackingButtonIdentifier];
    [_trackButton addTarget:self
                     action:@selector(trackButtonToggled)
           forControlEvents:UIControlEventTouchUpInside];
    [_priceTrackingStackView addArrangedSubview:_trackButton];
  }

  _priceTrackingStackView.axis = UILayoutConstraintAxisHorizontal;
  _priceTrackingStackView.spacing = kHorizontalStackViewSpacing;
  _priceTrackingStackView.distribution = UIStackViewDistributionFill;
  _priceTrackingStackView.alignment = UIStackViewAlignmentCenter;
  _priceTrackingStackView.translatesAutoresizingMaskIntoConstraints = NO;
  _priceTrackingStackView.backgroundColor =
      [UIColor colorNamed:kBackgroundColor];
  _priceTrackingStackView.layoutMarginsRelativeArrangement = YES;
  _priceTrackingStackView.layoutMargins =
      UIEdgeInsets(kContentVerticalInset, kContentHorizontalInset,
                   kContentVerticalInset, kContentHorizontalInset);
}

// Method that creates a view for the buying options module.
- (void)configureBuyingOptions {
  UILabel* title = [self createLabel];
  [title setAccessibilityIdentifier:kBuyingOptionsTitleIdentifier];
  title.font =
      CreateDynamicFont(UIFontTextStyleSubheadline, UIFontWeightSemibold);
  title.text = l10n_util::GetNSString(IDS_PRICE_INSIGHTS_BUYING_OPTIONS_TITLE);
  title.textColor = [UIColor colorNamed:kTextPrimaryColor];
  title.accessibilityTraits = UIAccessibilityTraitHeader;

  UILabel* subtitle = [self createLabel];
  [subtitle setAccessibilityIdentifier:kBuyingOptionsSubtitleIdentifier];
  subtitle.font =
      CreateDynamicFont(UIFontTextStyleSubheadline, UIFontWeightRegular);
  subtitle.text =
      l10n_util::GetNSString(IDS_PRICE_INSIGHTS_BUYING_OPTIONS_SUBTITLE);
  subtitle.textColor = [UIColor colorNamed:kTextSecondaryColor];

  UIStackView* verticalStack =
      [[UIStackView alloc] initWithArrangedSubviews:@[ title, subtitle ]];
  verticalStack.axis = UILayoutConstraintAxisVertical;
  verticalStack.distribution = UIStackViewDistributionFill;
  verticalStack.alignment = UIStackViewAlignmentLeading;
  verticalStack.spacing = kPriceTrackingVerticalStackViewSpacing;
  verticalStack.isAccessibilityElement = NO;

  UIImage* icon = DefaultSymbolWithPointSize(kOpenImageActionSymbol, kIconSize);
  UIImageView* iconView = [[UIImageView alloc] initWithImage:icon];
  iconView.tintColor = [UIColor colorNamed:kGrey500Color];
  iconView.isAccessibilityElement = NO;

  _buyingOptionsStackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ verticalStack, iconView ]];
  [_buyingOptionsStackView
      setAccessibilityIdentifier:kBuyingOptionsStackViewIdentifier];
  _buyingOptionsStackView.axis = UILayoutConstraintAxisHorizontal;
  _buyingOptionsStackView.spacing = kHorizontalStackViewSpacing;
  _buyingOptionsStackView.distribution = UIStackViewDistributionFill;
  _buyingOptionsStackView.alignment = UIStackViewAlignmentCenter;
  _buyingOptionsStackView.translatesAutoresizingMaskIntoConstraints = NO;
  _buyingOptionsStackView.backgroundColor =
      [UIColor colorNamed:kBackgroundColor];
  _buyingOptionsStackView.layoutMarginsRelativeArrangement = YES;
  _buyingOptionsStackView.layoutMargins =
      UIEdgeInsets(kContentVerticalInset, kContentHorizontalInset,
                   kContentVerticalInset, kContentHorizontalInset);
  _buyingOptionsStackView.isAccessibilityElement = YES;
  _buyingOptionsStackView.accessibilityTraits = UIAccessibilityTraitLink;
  _buyingOptionsStackView.accessibilityLabel =
      l10n_util::GetNSString(IDS_BUYING_OPTIONS_ACCESSIBILITY_DESCRIPTION);
  [_buyingOptionsStackView
      addInteraction:[[ViewPointerInteraction alloc] init]];

  UITapGestureRecognizer* tapRecognizer = [[UITapGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(handleBuyingOptionsTap:)];
  [_buyingOptionsStackView addGestureRecognizer:tapRecognizer];
}

// Method that creates a swiftUI graph for price history.
- (void)configurePriceHistoryWithTitle:(NSString*)titleText
                       primarySubtitle:(NSString*)primarySubtitleText
                     secondarySubtitle:(NSString*)secondarySubtitleText {
  UIStackView* verticalStack = [[UIStackView alloc] init];
  verticalStack.axis = UILayoutConstraintAxisVertical;
  verticalStack.distribution = UIStackViewDistributionFill;
  verticalStack.alignment = UIStackViewAlignmentLeading;
  verticalStack.spacing = kPriceTrackingVerticalStackViewSpacing;

  UILabel* title = [self createLabel];
  [title setAccessibilityIdentifier:kPriceHistoryTitleIdentifier];
  title.font =
      CreateDynamicFont(UIFontTextStyleSubheadline, UIFontWeightSemibold);
  title.text = titleText;
  title.textColor = [UIColor colorNamed:kTextPrimaryColor];
  title.accessibilityTraits = UIAccessibilityTraitHeader;
  [verticalStack addArrangedSubview:title];

  if (primarySubtitleText.length) {
    UILabel* primarySubtitle = [self createLabel];
    [primarySubtitle
        setAccessibilityIdentifier:kPriceHistoryPrimarySubtitleIdentifier];
    primarySubtitle.font =
        CreateDynamicFont(UIFontTextStyleFootnote, UIFontWeightRegular);
    primarySubtitle.text = primarySubtitleText;
    primarySubtitle.textColor = [UIColor colorNamed:kTextSecondaryColor];
    [verticalStack addArrangedSubview:primarySubtitle];

    // Set secondarySubtitle only if both primarySubtitle and
    // secondarySubtitle are present.
    if (secondarySubtitleText.length) {
      UILabel* secondarySubtitle = [self createLabel];
      [secondarySubtitle
          setAccessibilityIdentifier:kPriceHistorySecondarySubtitleIdentifier];
      secondarySubtitle.font =
          CreateDynamicFont(UIFontTextStyleFootnote, UIFontWeightRegular);
      secondarySubtitle.text = secondarySubtitleText;
      secondarySubtitle.textColor = [UIColor colorNamed:kTextSecondaryColor];
      [verticalStack addArrangedSubview:secondarySubtitle];
    }
  }

  UIViewController* priceHistoryViewController = [PriceHistoryProvider
      makeViewControllerWithHistory:self.item.priceHistory
                           currency:base::SysUTF8ToNSString(
                                        self.item.currency)];
  priceHistoryViewController.view.translatesAutoresizingMaskIntoConstraints =
      NO;
  [self.viewController addChildViewController:priceHistoryViewController];
  [priceHistoryViewController
      didMoveToParentViewController:self.viewController];
  [NSLayoutConstraint activateConstraints:@[
    [priceHistoryViewController.view.heightAnchor
        constraintEqualToConstant:kPriceHistoryGraphHeight]
  ]];

  _priceHistoryStackView = [[UIStackView alloc] initWithArrangedSubviews:@[
    verticalStack, priceHistoryViewController.view
  ]];
  [_priceHistoryStackView
      setAccessibilityIdentifier:kPriceHistoryStackViewIdentifier];
  _priceHistoryStackView.axis = UILayoutConstraintAxisVertical;
  _priceHistoryStackView.spacing = kPriceHistoryContentSpacing;
  _priceHistoryStackView.distribution = UIStackViewDistributionFill;
  _priceHistoryStackView.translatesAutoresizingMaskIntoConstraints = NO;
  _priceHistoryStackView.backgroundColor =
      [UIColor colorNamed:kBackgroundColor];
  _priceHistoryStackView.layoutMarginsRelativeArrangement = YES;
  _priceHistoryStackView.layoutMargins =
      UIEdgeInsets(kContentVerticalInset, kContentHorizontalInset,
                    kContentVerticalInset, kContentHorizontalInset);
}

- (void)configurePriceRangeWithTitle:(NSString*)titleText
                            subtitle:(NSString*)primarySubtitleText {
  UIStackView* labelStackView = [[UIStackView alloc] init];
  labelStackView.axis = UILayoutConstraintAxisVertical;
  labelStackView.distribution = UIStackViewDistributionFill;
  labelStackView.alignment = UIStackViewAlignmentLeading;
  labelStackView.spacing = kPriceTrackingVerticalStackViewSpacing;
  labelStackView.isAccessibilityElement = NO;

  UILabel* title = [self createLabel];
  [title setAccessibilityIdentifier:kPriceRangeTitleIdentifier];
  title.font =
      CreateDynamicFont(UIFontTextStyleSubheadline, UIFontWeightSemibold);
  title.text = titleText;
  title.numberOfLines = 2;
  title.textColor = [UIColor colorNamed:kTextPrimaryColor];
  [labelStackView addArrangedSubview:title];

  if (primarySubtitleText.length) {
    UILabel* primarySubtitle = [self createLabel];
    [primarySubtitle setAccessibilityIdentifier:kPriceRangeSubtitleIdentifier];
    primarySubtitle.font =
        CreateDynamicFont(UIFontTextStyleFootnote, UIFontWeightRegular);
    primarySubtitle.text = primarySubtitleText;
    primarySubtitle.textColor = [UIColor colorNamed:kTextSecondaryColor];
    [labelStackView addArrangedSubview:primarySubtitle];
  }

  std::u16string lowPriceFormatted = getFormattedCurrentPrice(
      self.item.lowPrice, self.item.currency, self.item.country);
  std::u16string highPriceFormatted = getFormattedCurrentPrice(
      self.item.highPrice, self.item.currency, self.item.country);
  CGRect contentArea = [UIScreen mainScreen].bounds;
  CGFloat sliderViewWidth = contentArea.size.width -
                            (kContentHorizontalInset * 2) -
                            (kHorizontalInset * 2);

  PriceRangeSliderView* sliderStackView = [[PriceRangeSliderView alloc]
      initWithMinimumLabelText:base::SysUTF16ToNSString(lowPriceFormatted)
              maximumLabelText:base::SysUTF16ToNSString(highPriceFormatted)
                  minimumValue:self.item.lowPrice
                  maximumValue:self.item.highPrice
                  currentValue:self.item.currentPrice
               sliderViewWidth:sliderViewWidth];
  sliderStackView.isAccessibilityElement = NO;

  _priceRangeStackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ labelStackView, sliderStackView ]];
  [_priceRangeStackView
      setAccessibilityIdentifier:kPriceRangeStackViewIdentifier];
  _priceRangeStackView.axis = UILayoutConstraintAxisVertical;
  _priceRangeStackView.spacing = kPriceRangeContentSpacing;
  _priceRangeStackView.distribution = UIStackViewDistributionFill;
  _priceRangeStackView.translatesAutoresizingMaskIntoConstraints = NO;
  _priceRangeStackView.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  _priceRangeStackView.layoutMarginsRelativeArrangement = YES;
  _priceRangeStackView.layoutMargins =
      UIEdgeInsets(kContentVerticalInset, kContentHorizontalInset,
                   kContentVerticalInset, kContentHorizontalInset);
  _priceRangeStackView.isAccessibilityElement = YES;
  std::u16string currentPriceFormatted = getFormattedCurrentPrice(
      self.item.currentPrice, self.item.currency, self.item.country);
  _priceRangeStackView.accessibilityLabel = l10n_util::GetNSStringF(
      IDS_PRICE_RANGE_ACCESSIBILITY_DESCRIPTION,
      base::SysNSStringToUTF16(self.item.title), lowPriceFormatted,
      highPriceFormatted, currentPriceFormatted);
  _priceRangeStackView.accessibilityTraits = UIAccessibilityTraitImage;
}

// Creates and configures a UILabel with default settings.
- (UILabel*)createLabel {
  UILabel* label = [[UILabel alloc] init];
  label.textAlignment = NSTextAlignmentLeft;
  label.adjustsFontForContentSizeCategory = YES;
  label.adjustsFontSizeToFitWidth = NO;
  label.lineBreakMode = NSLineBreakByTruncatingTail;
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.numberOfLines = 1;
  return label;
}

- (void)setOrUpdateTrackButton {
  UIFont* font =
      CreateDynamicFont(UIFontTextStyleSubheadline, UIFontWeightBold);
  NSDictionary* attributes = @{NSFontAttributeName : font};
  NSString* titleText =
      self.item.isPriceTracked
          ? l10n_util::GetNSString(IDS_PRICE_INSIGHTS_TRACKING_BUTTON_TITLE)
          : l10n_util::GetNSString(IDS_PRICE_INSIGHTS_TRACK_BUTTON_TITLE);
  NSMutableAttributedString* title =
      [[NSMutableAttributedString alloc] initWithString:titleText];
  [title addAttributes:attributes range:NSMakeRange(0, title.length)];

  if (!_trackButton) {
    UIButtonConfiguration* configuration =
        [UIButtonConfiguration plainButtonConfiguration];
    configuration.baseForegroundColor = [UIColor colorNamed:kSolidWhiteColor];
    configuration.background.backgroundColor = [UIColor colorNamed:kBlueColor];
    configuration.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
    configuration.contentInsets = NSDirectionalEdgeInsetsMake(
        kTrackButtonVerticalPadding, 0, kTrackButtonVerticalPadding, 0);
    _trackButton = [[UIButton alloc] init];
    _trackButton.configuration = configuration;
    _trackButtonWidthConstraint =
        [_trackButton.widthAnchor constraintEqualToConstant:0];
    _trackButtonWidthConstraint.active = YES;
    _trackButton.pointerInteractionEnabled = YES;
  }

  if (!self.item.isPriceTracked) {
    _trackButton.accessibilityLabel = l10n_util::GetNSString(
        IDS_PRICE_TRACKING_NOT_TRACKING_ACCESSIBILITY_DESCRIPTION);
  }

  [_trackButton setAttributedTitle:title forState:UIControlStateNormal];
  CGSize stringSize = [titleText sizeWithAttributes:attributes];
  _trackButtonWidthConstraint.constant =
      stringSize.width + kTrackButtonHorizontalPadding * 2;
}

#pragma mark - Actions

- (void)trackButtonToggled {
  if (self.item.isPriceTracked) {
    [self.mutator priceInsightsStopTrackingItem:self.item];
    return;
  }

  [self.mutator priceInsightsTrackItem:self.item];
}

- (void)handleBuyingOptionsTap:(UITapGestureRecognizer*)sender {
  [self.mutator priceInsightsNavigateToWebpageForItem:self.item];
}

@end
