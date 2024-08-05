// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/accounts_table_view_controller.h"

#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/accounts_table_view_controller_constants.h"

@interface AccountsTableViewController () {
  // Whether to close the view after adding an account.
  BOOL _closeSettingsOnAddAccount;

  // ApplicationCommands handler.
  id<ApplicationCommands> _applicationHandler;
}

@end

@implementation AccountsTableViewController

@synthesize modelIdentityDataSource;

- (instancetype)initWithCloseSettingsOnAddAccount:
                    (BOOL)closeSettingsOnAddAccount
                       applicationCommandsHandler:
                           (id<ApplicationCommands>)applicationCommandsHandler {
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _closeSettingsOnAddAccount = closeSettingsOnAddAccount;
    _applicationHandler = applicationCommandsHandler;
  }

  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.accessibilityIdentifier = kSettingsEditAccountListTableViewId;

  [self loadModel];
}

@end
