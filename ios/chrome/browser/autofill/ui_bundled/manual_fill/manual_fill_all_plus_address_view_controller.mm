// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_all_plus_address_view_controller.h"

#import "base/notreached.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_action_cell.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_constants.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_plus_address_cell.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ui/base/l10n/l10n_util_mac.h"

@implementation ManualFillAllPlusAddressViewController {
  // Search controller.
  UISearchController* _searchController;
}

- (instancetype)initWithSearchController:(UISearchController*)searchController {
  self = [super init];
  if (self) {
    _searchController = searchController;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.definesPresentationContext = YES;
  _searchController.searchBar.backgroundColor = [UIColor clearColor];
  _searchController.obscuresBackgroundDuringPresentation = NO;
  _searchController.searchBar.accessibilityIdentifier =
      manual_fill::kPlusAddressSearchBarAccessibilityIdentifier;
  self.navigationItem.searchController = _searchController;
  self.navigationItem.hidesSearchBarWhenScrolling = NO;
  self.title = l10n_util::GetNSString(IDS_SELECT_PLUS_ADDRESS_TITLE_IOS);
  self.navigationItem.largeTitleDisplayMode =
      UINavigationItemLargeTitleDisplayModeNever;

  UIBarButtonItem* doneButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(handleDoneButton)];
  doneButton.accessibilityIdentifier =
      manual_fill::kPlusAddressDoneButtonAccessibilityIdentifier;
  self.navigationItem.rightBarButtonItem = doneButton;
}

#pragma mark - ManualFillPlusAddressConsumer

- (void)presentPlusAddresses:
    (NSArray<ManualFillPlusAddressItem*>*)plusAddresses {
  [self presentDataItems:plusAddresses];
}

- (void)presentPlusAddressActions:(NSArray<ManualFillActionItem*>*)actions {
  NOTREACHED_NORETURN();
}

#pragma mark - Private

- (void)handleDoneButton {
  [self.delegate selectPlusAddressViewControllerDidTapDoneButton:self];
}

@end
