// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/shopping_table_view_controller.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/autofill_ai_base_item_type.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
// Section identifiers in the "Shopping" page table view.
enum SectionIdentifier {
  kOrderSection = kSectionIdentifierEnumZero,
  kShipmentSection,
};
}  // namespace

@implementation ShoppingTableViewController {
  NSArray<TableViewItem*>* _orders;
  NSArray<TableViewItem*>* _shipments;
}

- (instancetype)init {
  return [super initWithStyle:ChromeTableViewStyle()];
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = l10n_util::GetNSString(IDS_AUTOFILL_SHOPPING_TITLE);
  [self loadModel];
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.delegate shoppingTableViewControllerDidRemove:self];
  }
}

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;

  if (_orders.count > 0) {
    [model addSectionWithIdentifier:kOrderSection];
    TableViewTextHeaderFooterItem* header =
        [[TableViewTextHeaderFooterItem alloc]
            initWithType:kAutofillAIBaseItemTypeHeader];
    header.text = l10n_util::GetNSString(IDS_AUTOFILL_AI_ORDERS_TITLE);
    [model setHeader:header forSectionWithIdentifier:kOrderSection];
    for (TableViewItem* item in _orders) {
      [model addItem:item toSectionWithIdentifier:kOrderSection];
    }
  }

  if (_shipments.count > 0) {
    [model addSectionWithIdentifier:kShipmentSection];
    TableViewTextHeaderFooterItem* header =
        [[TableViewTextHeaderFooterItem alloc]
            initWithType:kAutofillAIBaseItemTypeHeader];
    header.text = l10n_util::GetNSString(IDS_AUTOFILL_AI_SHIPMENTS_TITLE);
    [model setHeader:header forSectionWithIdentifier:kShipmentSection];
    for (TableViewItem* item in _shipments) {
      [model addItem:item toSectionWithIdentifier:kShipmentSection];
    }
  }
}

#pragma mark - ShoppingConsumer

- (void)setShoppingWithOrders:(NSArray<TableViewItem*>*)orders
                    shipments:(NSArray<TableViewItem*>*)shipments {
  _orders = orders;
  _shipments = shipments;
  if (self.isViewLoaded) {
    [self reloadData];
  }
}

#pragma mark - SettingsRootTableViewController

- (BOOL)shouldHideToolbar {
  return YES;
}

@end
