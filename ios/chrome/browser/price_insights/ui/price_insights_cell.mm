// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/price_insights/ui/price_insights_cell.h"

#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/price_insights/ui/price_history_swift.h"
#import "ios/chrome/browser/price_insights/ui/price_insights_constants.h"
#import "ios/chrome/browser/price_insights/ui/price_insights_mutator.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
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

// Height of Price History graph.
const CGFloat kPriceHistoryGraphHeight = 186.0f;

// The corner radius of this container.
const float kCornerRadius = 24;

// The horizontal padding for the track button.
const CGFloat kTrackButtonHorizontalPadding = 14.0f;

// The vertical padding for the track button.
const CGFloat kTrackButtonVerticalPadding = 3.0f;

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
  if ([self hasPriceHistory] && self.item.currency) {
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

#pragma mark - Private

// Returns whether or not price range is available.
- (BOOL)hasPriceRange {
  return self.item.lowPrice.length > 0 && self.item.highPrice.length > 0;
}

// Returns whether or not price has one typical price. If there is only one
// typical price, the high and low price are equal.
- (BOOL)hasPriceOneTypicalPrice {
  return [self hasPriceRange]
             ? [self.item.highPrice isEqualToString:self.item.lowPrice]
             : NO;
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

  UILabel* priceTrackingSubtitle = [self createLabel];
  [priceTrackingSubtitle
      setAccessibilityIdentifier:kPriceTrackingSubtitleIdentifier];
  priceTrackingSubtitle.font =
      CreateDynamicFont(UIFontTextStyleSubheadline, UIFontWeightRegular);
  priceTrackingSubtitle.textColor = [UIColor colorNamed:kTextSecondaryColor];
  if ([self hasPriceRange] && [self hasPriceHistory]) {
    priceTrackingSubtitle.text =
        [self hasVariants]
            ? ([self hasPriceOneTypicalPrice]
                   ? l10n_util::GetNSStringF(
                         IDS_PRICE_RANGE_ALL_OPTIONS_ONE_TYPICAL_PRICE,
                         base::SysNSStringToUTF16(self.item.lowPrice))
                   : l10n_util::GetNSStringF(
                         IDS_PRICE_RANGE_ALL_OPTIONS,
                         base::SysNSStringToUTF16(self.item.lowPrice),
                         base::SysNSStringToUTF16(self.item.highPrice)))
            : ([self hasPriceOneTypicalPrice]
                   ? l10n_util::GetNSStringF(
                         IDS_PRICE_RANGE_SINGLE_OPTION_ONE_TYPICAL_PRICE,
                         base::SysNSStringToUTF16(self.item.lowPrice))
                   : l10n_util::GetNSStringF(
                         IDS_PRICE_RANGE_SINGLE_OPTION,
                         base::SysNSStringToUTF16(self.item.lowPrice),
                         base::SysNSStringToUTF16(self.item.highPrice)));
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

  UIImage* icon = DefaultSymbolWithPointSize(kOpenImageActionSymbol, kIconSize);
  UIImageView* iconView = [[UIImageView alloc] initWithImage:icon];
  iconView.tintColor = [UIColor colorNamed:kGrey500Color];

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

  UILabel* title = [self createLabel];
  [title setAccessibilityIdentifier:kPriceHistoryTitleIdentifier];
  title.font =
      CreateDynamicFont(UIFontTextStyleSubheadline, UIFontWeightSemibold);
  title.text = titleText;
  title.textColor = [UIColor colorNamed:kTextPrimaryColor];
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

  UIViewController* priceHistoryViewController =
      [PriceHistoryProvider makeViewControllerWithHistory:self.item.priceHistory
                                                 currency:self.item.currency];
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
    configuration.baseForegroundColor = UIColor.whiteColor;
    configuration.background.backgroundColor = [UIColor colorNamed:kBlueColor];
    configuration.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
    configuration.contentInsets = NSDirectionalEdgeInsetsMake(
        kTrackButtonVerticalPadding, kTrackButtonHorizontalPadding,
        kTrackButtonVerticalPadding, kTrackButtonHorizontalPadding);
    _trackButton = [[UIButton alloc] init];
    _trackButton.configuration = configuration;
    _trackButtonWidthConstraint =
        [_trackButton.widthAnchor constraintEqualToConstant:0];
    _trackButtonWidthConstraint.active = YES;
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
