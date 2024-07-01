// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_ACCOUNT_MENU_ACCOUNT_MENU_DATA_SOURCE_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_ACCOUNT_MENU_ACCOUNT_MENU_DATA_SOURCE_H_

#import <UIKit/UIKit.h>

#import <vector>

@class AccountErrorUIInfo;
@class AccountsTableViewController;
enum class IdentityAvatarSize;
@protocol SystemIdentity;
@class TableViewIdentityItem;

// Identity data source for AccountMenuTableViewController instance, to
// manage the model.
@protocol AccountMenuDataSource <NSObject>

// Provides error UI info to the controller.
@property(nonatomic, readonly) AccountErrorUIInfo* accountErrorUIInfo;

// The gaia ids of the secondary accounts.
@property(nonatomic, readonly) NSArray<NSString*>* secondaryAccountsGaiaIDs;

// The email of the primary account.
@property(nonatomic, readonly) NSString* primaryAccountEmail;

// The avatar of the primary account.
@property(nonatomic, readonly) UIImage* primaryAccountAvatar;

// The user full name of the primary account.
@property(nonatomic, readonly) NSString* primaryAccountUserFullName;

// The identity for the value at `itemIndex` in the table.
- (TableViewIdentityItem*)identityItemForGaiaID:(NSString*)gaiaID;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_ACCOUNT_MENU_ACCOUNT_MENU_DATA_SOURCE_H_
