// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/card_view_controller.h"

#import "base/ios/ios_util.h"
#import "base/metrics/histogram_macros.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_action_cell.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace manual_fill {

NSString* const CardTableViewAccessibilityIdentifier =
    @"kManualFillCardTableViewAccessibilityIdentifier";

}  // namespace manual_fill

@implementation CardViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.tableView.accessibilityIdentifier =
      manual_fill::CardTableViewAccessibilityIdentifier;
}

#pragma mark - ManualFillCardConsumer

- (void)presentCards:(NSArray<ManualFillCardItem*>*)cards {
  UMA_HISTOGRAM_COUNTS_100("ManualFallback.PresentedOptions.CreditCards",
                           cards.count);
  [self presentDataItems:(NSArray<TableViewItem*>*)cards];
}

- (void)presentActions:(NSArray<ManualFillActionItem*>*)actions {
  [self presentActionItems:actions];
}

@end
