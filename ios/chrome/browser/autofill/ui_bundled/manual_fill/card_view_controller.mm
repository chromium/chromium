// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/card_view_controller.h"

#import "base/ios/ios_util.h"
#import "base/metrics/histogram_macros.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_action_cell.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace manual_fill {

enum ManualFallbackItemType : NSInteger {
  kNoCardsMessage = kItemTypeEnumZero,
};

}  // namespace manual_fill

@implementation CardViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.tableView.accessibilityIdentifier =
      manual_fill::kCardTableViewAccessibilityIdentifier;
}

#pragma mark - ManualFillCardConsumer

- (void)presentCards:(NSArray<ManualFillCardItem*>*)cards {
  UMA_HISTOGRAM_COUNTS_100("ManualFallback.PresentedOptions.CreditCards",
                           cards.count);

  self.noRegularDataItemsToShowHeaderItem = nil;
  if (!cards.count && IsKeyboardAccessoryUpgradeEnabled()) {
    TableViewTextHeaderFooterItem* textHeaderFooterItem =
        [[TableViewTextHeaderFooterItem alloc]
            initWithType:manual_fill::ManualFallbackItemType::kNoCardsMessage];
    textHeaderFooterItem.text =
        l10n_util::GetNSString(IDS_IOS_MANUAL_FALLBACK_NO_PAYMENT_METHODS);
    self.noRegularDataItemsToShowHeaderItem = textHeaderFooterItem;
  }

  [self presentDataItems:(NSArray<TableViewItem*>*)cards];
}

- (void)presentActions:(NSArray<ManualFillActionItem*>*)actions {
  [self presentActionItems:actions];
}

@end
