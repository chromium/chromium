// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/catalogs/ui/view_catalog_view_controller.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/catalogs/ui/details_view_controllers/signin_promo_catalog_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"

namespace {

// Sections used in View Catalog page.
enum SectionIdentifier {
  kSectionIdentifierView = kSectionIdentifierEnumZero,
};

// Item types used per View section.
enum ItemType {
  kItemTypeSigninPromo = kItemTypeEnumZero,
};

}  // namespace

@implementation ViewCatalogViewController {
  base::WeakPtr<Browser> _browser;
}

- (instancetype)initWithBrowser:(Browser*)browser {
  self = [super initWithStyle:UITableViewStyleInsetGrouped];
  if (self) {
    _browser = browser->AsWeakPtr();
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.title = @"View Catalog";

  [self loadModel];
}

- (void)loadModel {
  [super loadModel];

  TableViewModel<TableViewItem*>* model = self.tableViewModel;

  TableViewTextItem* signinPromoItem =
      [[TableViewTextItem alloc] initWithType:kItemTypeSigninPromo];
  signinPromoItem.text = @"Signin Promo";

  // Add sections.
  [model addSectionWithIdentifier:kSectionIdentifierView];

  // Add items.
  [model addItem:signinPromoItem
      toSectionWithIdentifier:kSectionIdentifierView];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  switch ([self.tableViewModel itemTypeForIndexPath:indexPath]) {
    case kItemTypeSigninPromo: {
      SigninPromoCatalogViewController* viewController =
          [[SigninPromoCatalogViewController alloc]
              initWithBrowser:_browser.get()];
      [self.navigationController pushViewController:viewController
                                           animated:YES];
      break;
    }
  }
}

@end
