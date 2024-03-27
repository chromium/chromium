// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/price_insights/ui/price_insights_cell.h"

#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/price_notifications/cells/price_notifications_track_button.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

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
const CGFloat kPriceTrackingStackViewSpacing = 20.0f;

// The corner radius of this container.
const float kCornerRadius = 24;

}  // namespace

@interface PriceInsightsCell ()

// Object with data related to price insights.
@property(nonatomic, strong) PriceInsightsItem* item;

@end

@implementation PriceInsightsCell {
  PriceNotificationsTrackButton* _trackButton;
  UIStackView* _priceTrackingStackView;
  UIStackView* _contentStackView;
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

    [self.contentView addSubview:_contentStackView];
    AddSameConstraintsWithInsets(
        _contentStackView, self.contentView,
        NSDirectionalEdgeInsetsMake(0, kHorizontalInset, 0, -kHorizontalInset));
  }
  return self;
}

- (void)configureWithItem:(PriceInsightsItem*)item {
  self.item = item;
  if (self.item.canPriceTrack ||
      ([self hasPriceRange] && [self hasPriceHistory])) {
    [self configurePriceTrackingAndRange];
    [_contentStackView addArrangedSubview:_priceTrackingStackView];
  }
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
  return NO;
}

// Method that creates a view for both price tracking and price range, or solely
// for price tracking or price range when price history is also available.
- (void)configurePriceTrackingAndRange {
  UILabel* priceTrackingTitle = [[UILabel alloc] init];
  priceTrackingTitle.numberOfLines = 1;
  priceTrackingTitle.textAlignment = NSTextAlignmentLeft;
  priceTrackingTitle.font =
      CreateDynamicFont(UIFontTextStyleSubheadline, UIFontWeightSemibold);
  priceTrackingTitle.adjustsFontForContentSizeCategory = YES;
  priceTrackingTitle.adjustsFontSizeToFitWidth = NO;
  priceTrackingTitle.translatesAutoresizingMaskIntoConstraints = NO;
  priceTrackingTitle.lineBreakMode = NSLineBreakByTruncatingTail;
  priceTrackingTitle.textColor = [UIColor colorNamed:kTextPrimaryColor];
  priceTrackingTitle.text = self.item.title;

  UILabel* priceTrackingSubtitle = [[UILabel alloc] init];
  priceTrackingSubtitle.textAlignment = NSTextAlignmentLeft;
  priceTrackingSubtitle.font =
      CreateDynamicFont(UIFontTextStyleSubheadline, UIFontWeightRegular);
  priceTrackingSubtitle.adjustsFontForContentSizeCategory = YES;
  priceTrackingSubtitle.adjustsFontSizeToFitWidth = NO;
  priceTrackingSubtitle.lineBreakMode = NSLineBreakByTruncatingTail;
  priceTrackingSubtitle.translatesAutoresizingMaskIntoConstraints = NO;
  priceTrackingSubtitle.textColor = [UIColor colorNamed:kTextSecondaryColor];
  if ([self hasPriceRange] && [self hasPriceHistory]) {
    priceTrackingSubtitle.numberOfLines = 1;
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
  [_priceTrackingStackView addArrangedSubview:verticalStack];

  if (self.item.canPriceTrack) {
    _trackButton = [[PriceNotificationsTrackButton alloc] init];
    [_trackButton addTarget:self
                     action:@selector(trackButtonToggled)
           forControlEvents:UIControlEventTouchUpInside];
    [_priceTrackingStackView addArrangedSubview:_trackButton];
  }

  _priceTrackingStackView.axis = UILayoutConstraintAxisHorizontal;
  _priceTrackingStackView.spacing = kPriceTrackingStackViewSpacing;
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

#pragma mark - Actions

- (void)trackButtonToggled {
}

@end
