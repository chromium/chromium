// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/commerce/ui_bundled/price_card/price_card_view.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/commerce/ui_bundled/price_card/price_card_constants.h"
#import "ios/chrome/browser/commerce/ui_bundled/price_card/resources/semantic_color_names.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface PriceCardView ()
// Formatted string representing the current price of the product represented by
// this PriceCardView.
@property(nonatomic, copy) NSString* currentPrice;
// Formatted string representing the previous price of the product represented
// by this PriceCardView.
@property(nonatomic, copy) NSString* previousPrice;
// Label containing the current price. The PriceCardView is composed of this
// label.
@property(nonatomic, weak) UILabel* currentPriceLabel;
// Label containing the previous price. The PriceCardView is coomposed of this
// label.
@property(nonatomic, weak) UILabel* previousPriceLabel;
@end

@implementation PriceCardView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  [super willMoveToSuperview:newSuperview];
  // The first time this moves to a superview, perform the view setup.
  if (newSuperview && self.subviews.count == 0) {
    [self setupViews];
  }
}

- (void)setupViews {
  UILabel* currentPriceLabel = [[UILabel alloc] init];
  self.translatesAutoresizingMaskIntoConstraints = NO;
  self.backgroundColor = [UIColor colorNamed:kStaticGreen50Color];
  self.layer.cornerRadius = kPriceCardCornerRadius;

  currentPriceLabel.translatesAutoresizingMaskIntoConstraints = NO;
  currentPriceLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  currentPriceLabel.textColor = [UIColor colorNamed:kStaticGreen700Color];
  currentPriceLabel.backgroundColor = [UIColor colorNamed:kStaticGreen50Color];
  currentPriceLabel.adjustsFontForContentSizeCategory = YES;
  _currentPriceLabel = currentPriceLabel;

  UILabel* previousPriceLabel = [[UILabel alloc] init];
  previousPriceLabel.translatesAutoresizingMaskIntoConstraints = NO;
  previousPriceLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  previousPriceLabel.textColor =
      [UIColor colorNamed:kPriceDropPreviousPriceTextColor];
  previousPriceLabel.adjustsFontForContentSizeCategory = YES;
  previousPriceLabel.backgroundColor = [UIColor colorNamed:kStaticGreen50Color];
  _previousPriceLabel = previousPriceLabel;

  [self addSubview:currentPriceLabel];
  [self addSubview:previousPriceLabel];

  NSArray* constraints = @[
    [currentPriceLabel.topAnchor constraintEqualToAnchor:self.topAnchor
                                                constant:kPriceCardTopIndent],
    [currentPriceLabel.leadingAnchor
        constraintEqualToAnchor:self.leadingAnchor
                       constant:kPriceCardLeadingIndent],
    [previousPriceLabel.topAnchor
        constraintEqualToAnchor:currentPriceLabel.topAnchor],
    [previousPriceLabel.leadingAnchor
        constraintEqualToAnchor:currentPriceLabel.trailingAnchor
                       constant:kPriceCardPricePreviousPriceSpacing],
    [self.trailingAnchor
        constraintEqualToAnchor:previousPriceLabel.trailingAnchor
                       constant:kPriceCardLeadingIndent],
    [self.bottomAnchor constraintEqualToAnchor:previousPriceLabel.bottomAnchor
                                      constant:kPriceCardBottomIndent],
  ];
  [NSLayoutConstraint activateConstraints:constraints];
  self.hidden = YES;
}

- (void)setPriceDrop:(NSString*)currentPrice
       previousPrice:(NSString*)previousPrice {
  _currentPrice = currentPrice;
  self.currentPriceLabel.text = currentPrice;

  NSDictionary* attributes = @{
    NSStrikethroughStyleAttributeName :
        [NSNumber numberWithInt:NSUnderlineStyleSingle]
  };
  NSAttributedString* attrText =
      [[NSAttributedString alloc] initWithString:previousPrice
                                      attributes:attributes];
  self.previousPriceLabel.attributedText = attrText;
  _previousPrice = previousPrice;
  self.accessibilityLabel = l10n_util::GetNSStringF(
      IDS_IOS_TAB_SWITCHER_PRICE_CARD, base::SysNSStringToUTF16(currentPrice),
      base::SysNSStringToUTF16(previousPrice));
  self.hidden = FALSE;
}

- (BOOL)isAccessibilityElement {
  return YES;
}

@end
