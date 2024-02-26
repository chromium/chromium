// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_ACCOUNTS_ACCOUNTS_MODEL_IDENTITY_DATA_SOURCE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_ACCOUNTS_ACCOUNTS_MODEL_IDENTITY_DATA_SOURCE_H_

#include <vector>

@class AccountsTableViewController;
struct CoreAccountInfo;
@class AccountErrorUIInfo;
enum class IdentityAvatarSize;
@protocol SystemIdentity;

// Identity data source for AccountsTableViewController instance, to manage the
// model.
@protocol AccountsModelIdentityDataSource <NSObject>

// Provides identity info for an account.
- (id<SystemIdentity>)identityForAccount:(CoreAccountInfo)account;

// Provides identity avatar.
- (UIImage*)identityAvatarWithSizeForIdentity:(id<SystemIdentity>)identity
                                         size:(IdentityAvatarSize)size;

// Returns YES if the account is signed in not syncing, NO otherwise.
- (BOOL)isAccountSignedInNotSyncing;

// Provides error UI info to the controller.
- (AccountErrorUIInfo*)accountErrorUIInfo;

// Provides the information of all accounts that have refresh tokens.
- (std::vector<CoreAccountInfo>)accountsWithRefreshTokens;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_ACCOUNTS_ACCOUNTS_MODEL_IDENTITY_DATA_SOURCE_H_
