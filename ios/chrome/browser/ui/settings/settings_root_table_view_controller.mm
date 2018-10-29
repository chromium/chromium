// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/material_components/utils.h"
#import "ios/chrome/browser/ui/settings/cells/settings_cells_constants.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation SettingsRootTableViewController

@synthesize dispatcher = _dispatcher;

- (void)viewDidLoad {
  self.styler.tableViewBackgroundColor =
      [UIColor groupTableViewBackgroundColor];
  self.styler.tableViewSectionHeaderBlurEffect = nil;
  [super viewDidLoad];
  self.styler.cellBackgroundColor = [UIColor whiteColor];
  self.styler.cellTitleColor = [UIColor blackColor];
  self.tableView.estimatedRowHeight = kSettingsCellDefaultHeight;
  // Do not set the estimated height of the footer/header as if there is no
  // header/footer, there is an empty space.
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  // Set up the "Done" button in the upper right of the nav bar.
  SettingsNavigationController* navigationController =
      base::mac::ObjCCast<SettingsNavigationController>(
          self.navigationController);
  UIBarButtonItem* doneButton = [navigationController doneButton];
  self.navigationItem.rightBarButtonItem = doneButton;
}

#pragma mark - TableViewLinkHeaderFooterItemDelegate

- (void)view:(TableViewLinkHeaderFooterView*)view didTapLinkURL:(GURL)URL {
  // Subclass must have a valid dispatcher assigned.
  DCHECK(self.dispatcher);
  OpenNewTabCommand* command = [OpenNewTabCommand commandWithURLFromChrome:URL];
  [self.dispatcher closeSettingsUIAndOpenURL:command];
}

@end
