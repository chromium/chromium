// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/price_notifications/cells/price_notifications_price_chip_view.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/price_notifications/price_notifications_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
const CGFloat kPriceChipVerticalIndent = 2.0;
const CGFloat kPriceChipLeadingIndent = 8.0;
const CGFloat kPriceChipHorizontalSpacing = 4.0;
}  // namespace

@interface PriceNotificationsPriceChipView ()
// Label containing the current price.
@property(nonatomic, strong) UILabel* currentPriceLabel;
// Label containing the previous price.
@property(nonatomic, strong) UILabel* previousPriceLabel;

@end

@implementation PriceNotificationsPriceChipView

- (void)setPriceDrop:(NSString*)currentPrice
       previousPrice:(NSString*)previousPrice {
  [self prepareForReuse];

  self.currentPriceLabel.text = currentPrice;
  self.previousPriceLabel.text = previousPrice;

  [self addSubview:self.previousPriceLabel];

  // The leading anchor will be constrained later based on `previousPrice`
  // existence.
  [NSLayoutConstraint activateConstraints:@[
    [self.previousPriceLabel.topAnchor
        constraintEqualToAnchor:self.topAnchor
                       constant:kPriceChipVerticalIndent],
    [self.trailingAnchor
        constraintGreaterThanOrEqualToAnchor:self.previousPriceLabel
                                                 .trailingAnchor
                                    constant:kPriceChipLeadingIndent],
    [self.bottomAnchor
        constraintEqualToAnchor:self.previousPriceLabel.bottomAnchor
                       constant:kPriceChipVerticalIndent],
  ]];

  self.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_CHIP);
  if (currentPrice) {
    self.accessibilityValue = l10n_util::GetNSStringF(
        IDS_IOS_TAB_SWITCHER_PRICE_CARD,
        base::SysNSStringToUTF16(self.currentPriceLabel.text),
        base::SysNSStringToUTF16(self.previousPriceLabel.text));
    [self addSubview:self.currentPriceLabel];
    [NSLayoutConstraint activateConstraints:@[
      [self.previousPriceLabel.leadingAnchor
          constraintGreaterThanOrEqualToAnchor:self.currentPriceLabel
                                                   .trailingAnchor
                                      constant:kPriceChipHorizontalSpacing],

      [self.currentPriceLabel.topAnchor
          constraintEqualToAnchor:self.topAnchor
                         constant:kPriceChipVerticalIndent],
      [self.currentPriceLabel.leadingAnchor
          constraintEqualToAnchor:self.leadingAnchor
                         constant:kPriceChipLeadingIndent],
      [self.bottomAnchor
          constraintEqualToAnchor:self.currentPriceLabel.bottomAnchor
                         constant:kPriceChipVerticalIndent],
    ]];
    self.backgroundColor = [UIColor colorNamed:kGreen100Color];
  } else {
    self.accessibilityValue = self.previousPriceLabel.text;
    self.backgroundColor = [UIColor colorNamed:kGrey100Color];
    [self.previousPriceLabel.leadingAnchor
        constraintEqualToAnchor:self.leadingAnchor
                       constant:kPriceChipLeadingIndent]
        .active = YES;
  }
}

#pragma mark - Properties

- (UILabel*)currentPriceLabel {
  if (!_currentPriceLabel) {
    UILabel* priceLabel = [[UILabel alloc] init];
    priceLabel.translatesAutoresizingMaskIntoConstraints = NO;
    priceLabel.adjustsFontForContentSizeCategory = YES;
    priceLabel.font =
        CreateDynamicFont(UIFontTextStyleFootnote, UIFontWeightBold);
    priceLabel.textColor = [UIColor colorNamed:kGreen800Color];
    _currentPriceLabel = priceLabel;
  }
  return _currentPriceLabel;
}

- (UILabel*)previousPriceLabel {
  if (!_previousPriceLabel) {
    UILabel* priceLabel = [[UILabel alloc] init];
    priceLabel.translatesAutoresizingMaskIntoConstraints = NO;
    priceLabel.adjustsFontForContentSizeCategory = YES;
    priceLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    priceLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _previousPriceLabel = priceLabel;
  }
  return _previousPriceLabel;
}

#pragma mark - UIView

- (void)layoutSubviews {
  [super layoutSubviews];
  self.layer.cornerRadius = self.bounds.size.height / 2.0;
  self.clipsToBounds = YES;
}

#pragma mark - Private

// Resets the price chip by removing the chip's UILabels from the view.
- (void)prepareForReuse {
  [self.currentPriceLabel removeFromSuperview];
  [self.previousPriceLabel removeFromSuperview];
}

@end
