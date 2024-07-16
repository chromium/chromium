// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_browsing_data_view_controller.h"

#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/cells/clear_browsing_data_constants.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_browsing_data_view_controller_delegate.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@implementation QuickDeleteBrowsingDataViewController

#pragma mark - ChromeTableViewController

- (instancetype)init {
  UITableViewStyle style = ChromeTableViewStyle();
  self = [super initWithStyle:style];
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.navigationItem.largeTitleDisplayMode =
      UINavigationItemLargeTitleDisplayModeNever;
  self.navigationItem.leftBarButtonItem = [self cancelButton];
  self.navigationItem.rightBarButtonItem = [self confirmButton];
  self.title = l10n_util::GetNSString(IDS_IOS_DELETE_BROWSING_DATA_TITLE);
}

#pragma mark - Private

// Returns the cancel button on the navigation bar.
- (UIBarButtonItem*)cancelButton {
  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(onCancel:)];
  return cancelButton;
}

// Returns the confirm button on the navigation bar.
- (UIBarButtonItem*)confirmButton {
  UIBarButtonItem* confirmButton = [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_DELETE_BROWSING_DATA_CONFIRM)
              style:UIBarButtonItemStylePlain
             target:self
             action:@selector(onConfirm:)];
  confirmButton.accessibilityIdentifier =
      kQuickDeleteBrowsingDataConfirmButtonIdentifier;
  return confirmButton;
}

// Dismisses the page without saving changes in selection.
- (void)onCancel:(id)sender {
  [_delegate dismissBrowsingDataPage];
}

// Notifies the mutator of the confirmation of the browsing data types
// selection.
- (void)onConfirm:(id)sender {
  // TODO(crbug.com/341107834): Update changes in data types selection here.
  [_delegate dismissBrowsingDataPage];
}

@end
