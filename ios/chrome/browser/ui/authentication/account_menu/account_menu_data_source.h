// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_ACCOUNT_MENU_ACCOUNT_MENU_DATA_SOURCE_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_ACCOUNT_MENU_ACCOUNT_MENU_DATA_SOURCE_H_

#import <UIKit/UIKit.h>

@class AccountErrorUIInfo;
struct ManagementState;

// Identity data source for AccountMenuViewController instance, to
// manage the model.
@protocol AccountMenuDataSource <NSObject>

// Provides error UI info to the controller.
@property(nonatomic, readonly) AccountErrorUIInfo* accountErrorUIInfo;

// The gaia ids of the secondary accounts.
@property(nonatomic, readonly) NSArray<NSString*>* secondaryAccountsGaiaIDs;

// The email of the primary account. Not nil.
@property(nonatomic, readonly) NSString* primaryAccountEmail;

// The avatar of the primary account. Not nil.
@property(nonatomic, readonly) UIImage* primaryAccountAvatar;

// The user full name of the primary account. May be nil.
@property(nonatomic, readonly) NSString* primaryAccountUserFullName;

// The management state of this browser and profile.
@property(nonatomic, readonly) ManagementState managementState;

// The full name for the user with `gaiaID`.
- (NSString*)nameForGaiaID:(NSString*)gaiaID;

// The email for the user with `gaiaID`.
- (NSString*)emailForGaiaID:(NSString*)gaiaID;

// The image for the user with `gaiaID`.
- (UIImage*)imageForGaiaID:(NSString*)gaiaID;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_ACCOUNT_MENU_ACCOUNT_MENU_DATA_SOURCE_H_
