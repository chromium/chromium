// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/shopping_table_view_controller.h"

#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"

@implementation ShoppingTableViewController

- (instancetype)init {
  return [super initWithStyle:ChromeTableViewStyle()];
}

- (void)setShoppingWithOrders:(NSArray<TableViewItem*>*)orders
                    shipments:(NSArray<TableViewItem*>*)shipments {
}
- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.delegate shoppingTableViewControllerDidRemove:self];
  }
}

@end
