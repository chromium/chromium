// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_ACCOUNT_MENU_UI_ACCOUNT_MENU_DATA_SOURCE_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_ACCOUNT_MENU_UI_ACCOUNT_MENU_DATA_SOURCE_H_

#import <UIKit/UIKit.h>

#import <vector>

@class AccountErrorUIInfo;
class GaiaId;

// Identity data source for AccountMenuViewController instance, to
// manage the model.
@protocol AccountMenuDataSource <NSObject>

// Provides error UI info to the controller.
@property(nonatomic, readonly) AccountErrorUIInfo* accountErrorUIInfo;

// The gaia ids of the secondary accounts.
@property(nonatomic, readonly) std::vector<GaiaId> secondaryAccountsGaiaIDs;

// The email of the primary account. Not nil.
@property(nonatomic, readonly) NSString* primaryAccountEmail;

// The avatar of the primary account. Not nil.
@property(nonatomic, readonly) UIImage* primaryAccountAvatar;

// The user full name of the primary account. May be nil.
@property(nonatomic, readonly) NSString* primaryAccountUserFullName;

// The description showed when the browser is managed.
@property(nonatomic, readonly) NSString* managementDescription;

// The full name for the user with `gaiaID`.
- (NSString*)nameForGaiaID:(const GaiaId&)gaiaID;

// The email for the user with `gaiaID`.
- (NSString*)emailForGaiaID:(const GaiaId&)gaiaID;

// The image for the user with `gaiaID`.
- (UIImage*)imageForGaiaID:(const GaiaId&)gaiaID;

// Returns true if `gaiaID` is managed by checking for a hosted domain.
- (BOOL)isGaiaIDManaged:(const GaiaId&)gaiaID;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_ACCOUNT_MENU_UI_ACCOUNT_MENU_DATA_SOURCE_H_
