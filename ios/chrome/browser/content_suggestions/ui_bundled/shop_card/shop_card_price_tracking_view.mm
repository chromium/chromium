// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/shop_card/shop_card_price_tracking_view.h"

#import "base/strings/sys_string_conversions.h"
#import "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/shop_card/shop_card_data.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/tab_resumption/tab_resumption_commands.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/tab_resumption/tab_resumption_item.h"
#import "ios/chrome/browser/price_notifications/ui_bundled/cells/price_notifications_track_button.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "url/gurl.h"

namespace {

const CGFloat kVerticalStackSpacing = 6.0f;
const CGFloat kContentStackSpacing = 2.0f;

}  // namespace

@implementation ShopCardPriceTrackingView {
  // Item used to configure the view.
  TabResumptionItem* _item;

  UIStackView* _textStack;
  UILabel* _titleLabel;
  UILabel* _urlLabel;

  UIStackView* _contentStack;
  UIButton* _trackPriceButton;
}

- (instancetype)initWithItem:(TabResumptionItem*)item {
  self = [super init];
  if (self) {
    _item = item;
  }

  _trackPriceButton =
      [[PriceNotificationsTrackButton alloc] initWithLightVariant:YES];
  [_trackPriceButton addTarget:self
                        action:@selector(trackItem)
              forControlEvents:UIControlEventTouchUpInside];

  // Lay out text stack
  [self populateTitleLabel];
  [self populateUrlLabel];
  _textStack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ _titleLabel, _urlLabel ]];
  _textStack.axis = UILayoutConstraintAxisVertical;
  _textStack.alignment = UIStackViewAlignmentLeading;
  _textStack.spacing = kVerticalStackSpacing;

  _contentStack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ _textStack, _trackPriceButton ]];
  _contentStack.translatesAutoresizingMaskIntoConstraints = NO;
  _contentStack.spacing = kContentStackSpacing;
  _contentStack.alignment = UIStackViewAlignmentCenter;
  _contentStack.axis = UILayoutConstraintAxisHorizontal;
  [self addSubview:_contentStack];
  AddSameConstraints(_contentStack, self);

  return self;
}

- (void)populateTitleLabel {
  _titleLabel = [[UILabel alloc] init];
  _titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  _titleLabel.numberOfLines = 1;
  _titleLabel.lineBreakMode = NSLineBreakByTruncatingTail;
  _titleLabel.font =
      PreferredFontForTextStyle(UIFontTextStyleFootnote, UIFontWeightSemibold);
  _titleLabel.adjustsFontForContentSizeCategory = YES;
  _titleLabel.text = _item.tabTitle;
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

@end
