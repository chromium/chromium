// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_ACCOUNT_MENU_UI_ACCOUNT_MENU_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_ACCOUNT_MENU_UI_ACCOUNT_MENU_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/authentication/account_menu/ui/account_menu_consumer.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"

@protocol AccountMenuDataSource;
@protocol AccountMenuMutator;

@interface AccountMenuViewController : UIViewController <AccountMenuConsumer>

// The mutator for the account menu.
@property(nonatomic, weak) id<AccountMenuMutator> mutator;

// The data source for the account menu.
@property(nonatomic, weak) id<AccountMenuDataSource> dataSource;

- (instancetype)initWithHideEllipsisMenu:(BOOL)hideEllipsisMenu
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

- (instancetype)init NS_UNAVAILABLE;

// Sets up the sheet presentation controller and its properties.
// It must be called before the view is presented, in order to work on
// popover adapted as sheet on compact width tablets.
- (void)setUpBottomSheetPresentationController;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_ACCOUNT_MENU_UI_ACCOUNT_MENU_VIEW_CONTROLLER_H_
