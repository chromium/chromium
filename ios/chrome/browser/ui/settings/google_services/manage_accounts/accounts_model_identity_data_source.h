// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_ACCOUNTS_ACCOUNTS_MODEL_IDENTITY_DATA_SOURCE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_ACCOUNTS_ACCOUNTS_MODEL_IDENTITY_DATA_SOURCE_H_

#include <vector>

@class AccountErrorUIInfo;
struct CoreAccountInfo;
enum class IdentityAvatarSize;
@class IdentityViewItem;
@class LegacyAccountsTableViewController;
@protocol SystemIdentity;

// Identity data source for AccountsTableViewController instance, to manage the
// model.
@protocol AccountsModelIdentityDataSource <NSObject>

// Provides identity info with gaiaID.
- (id<SystemIdentity>)identityWithGaiaID:(NSString*)gaiaID;

// Provides identity avatar.
- (UIImage*)identityAvatarWithSizeForIdentity:(id<SystemIdentity>)identity
                                         size:(IdentityAvatarSize)size;

// Returns YES if the account is signed in not syncing, NO otherwise.
- (BOOL)isAccountSignedInNotSyncing;

// Provides error UI info to the controller.
- (AccountErrorUIInfo*)accountErrorUIInfo;

// Returns the primary identity view item.
- (IdentityViewItem*)primaryIdentityViewItem;

// Provides identity view items for all available identities.
- (std::vector<IdentityViewItem*>)identityViewItems;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_ACCOUNTS_ACCOUNTS_MODEL_IDENTITY_DATA_SOURCE_H_
