// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/inline_promo_item.h"

#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/settings/cells/inline_promo_cell.h"

@implementation InlinePromoItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [InlinePromoCell class];
    _shouldShowCloseButton = YES;
    _enabled = YES;
  }
  return self;
}

#pragma mark - TableViewItem

- (void)configureCell:(InlinePromoCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  cell.closeButton.hidden = !self.shouldShowCloseButton;
  cell.promoImageView.image = self.promoImage;
  cell.promoTextLabel.text = self.promoText;
  cell.enabled = self.enabled;
  cell.shouldHaveWideLayout = self.shouldHaveWideLayout;

  UIButtonConfiguration* buttonConfiguration =
      cell.moreInfoButton.configuration;
  UIFont* font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  NSDictionary* attributes = @{NSFontAttributeName : font};
  NSMutableAttributedString* attributedString =
      [[NSMutableAttributedString alloc] initWithString:self.moreInfoButtonTitle
                                             attributes:attributes];
  buttonConfiguration.attributedTitle = attributedString;
  cell.moreInfoButton.configuration = buttonConfiguration;

  cell.selectionStyle = UITableViewCellSelectionStyleNone;
}

@end
