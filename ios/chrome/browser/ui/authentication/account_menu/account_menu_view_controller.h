// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_ACCOUNT_MENU_ACCOUNT_MENU_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_ACCOUNT_MENU_ACCOUNT_MENU_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"
#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_consumer.h"

@protocol AccountMenuDataSource;
@protocol AccountMenuMutator;

// Identifier for the "manage your account" menu entry.
extern const char kManageYourGoogleAccountIdentifier[];
// Identifier for the "Edit account list" menu entry.
extern const char kEditAccountListIdentifier[];

@interface AccountMenuViewController
    : ChromeTableViewController <AccountMenuConsumer>

// The mutator for the account menu.
@property(nonatomic, weak) id<AccountMenuMutator> mutator;

// The data source for the account menu.
@property(nonatomic, weak) id<AccountMenuDataSource> dataSource;

// Sets up the sheet presentation controller and its properties.
// It must be called before the view is presented, in order to work on
// popover adapted as sheet on compact width tablets.
- (void)setUpBottomSheetPresentationController;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_ACCOUNT_MENU_ACCOUNT_MENU_VIEW_CONTROLLER_H_
