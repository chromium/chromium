// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PASSWORD_PASSWORD_SETTINGS_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PASSWORD_PASSWORD_SETTINGS_APP_INTERFACE_H_

#import <UIKit/UIKit.h>

#import "components/password_manager/core/browser/leak_detection/bulk_leak_check_service_interface.h"

// EarlGreyScopedBlockSwizzlerAppInterface contains the app-side
// implementation for helpers. These helpers are compiled into
// the app binary and can be called from either app or test code.
@interface PasswordSettingsAppInterface : NSObject

#pragma mark - Password Store Management

// Removes all credentials stored in the profile store. Returns YES on success.
+ (BOOL)clearProfilePasswordStore;

// Removes all credentials from both the profile and the account store. Returns
// YES on success.
+ (BOOL)clearPasswordStores;

// Creates `count` password forms with index being part of the username,
// password, origin and realm in the profile store.
+ (void)saveExamplePasswordToProfileWithCount:(NSInteger)count;

// Creates password form for `password`, `username`, and `origin` in the profile
// store. Returns YES on success.
+ (BOOL)saveExamplePasswordToProfileStore:(NSString*)password
                                 username:(NSString*)username
                                   origin:(NSString*)origin;

// Creates password form for `password`, `username`, and `origin` and saves it
// in the account store. Returns YES on success.
+ (BOOL)saveExamplePasswordToAccountStore:(NSString*)password
                                 username:(NSString*)username
                                   origin:(NSString*)origin;

// Creates a compromised password form for `password`, `username`, and `origin`
// in profile store. Returns YES on success.
+ (BOOL)saveCompromisedPasswordToProfileStore:(NSString*)password
                                     username:(NSString*)username
                                       origin:(NSString*)origin;

// Creates a muted compromised password form for `password`, `userName`, and
// `origin` in profile store. Returns YES on success.
+ (BOOL)saveMutedCompromisedPasswordToProfileStore:(NSString*)password
                                          username:(NSString*)userName
                                            origin:(NSString*)origin;

#pragma mark - Passkeys, Leak Check, and Settings State

// Dismisses snack bar.  Used before next test.
+ (void)dismissSnackBar;

// Removes all passkeys from the passkey store.
+ (void)clearPasskeyStore;

// Creates password form in profile store for `note`, `password`, `username`,
// and `origin`. Returns YES on success.
+ (BOOL)saveExampleNoteToProfileStore:(NSString*)note
                             password:(NSString*)password
                             username:(NSString*)username
                               origin:(NSString*)origin;

// Creates a blocked password form for `origin` in profile store. Returns YES on
// success.
+ (BOOL)saveExampleBlockedOriginToProfileStore:(NSString*)origin;

// Creates a federated password form for `federatedOrigin`, `username`, and
// `origin` in the profile store. Returns YES on success.
+ (BOOL)saveExampleFederatedOriginToProfileStore:(NSString*)federatedOrigin
                                        username:(NSString*)username
                                          origin:(NSString*)origin;

// Creates a passkey for `rpId`, `userId`, `username`, and `userDisplayName` in
// the passkey store.
+ (void)saveExamplePasskeyToStore:(NSString*)rpId
                           userId:(NSString*)userId
                         username:(NSString*)username
                  userDisplayName:(NSString*)userDisplayName;

// Creates a hidden passkey for `rpId`, `userId`, `username`, and
// `userDisplayName` in the passkey store.
+ (void)saveHiddenPasskeyToStore:(NSString*)rpId
                          userId:(NSString*)userId
                        username:(NSString*)username
                 userDisplayName:(NSString*)userDisplayName;

// Returns the number of password forms stored in the profile store.
+ (NSInteger)passwordProfileStoreResultsCount;

// Returns YES if credential service is enabled.
+ (BOOL)isCredentialsServiceEnabled;

// Returns YES if automatic passkey upgrades are enabled
+ (BOOL)isAutomaticPasskeyUpgradesEnabled;

// Sets the FakeBulkLeakCheck's buffered `state`.
+ (void)setFakeBulkLeakCheckBufferedState:
    (password_manager::BulkLeakCheckServiceInterface::State)state;

// Sets the FakeBulkLeakCheck's buffered `state` and makes the
// FakeBulkLeakCheckService propagate it to its observers.
+ (void)setFakeBulkLeakCheckBufferedStateAndNotifyObservers:
    (password_manager::BulkLeakCheckServiceInterface::State)state;

// Returns YES if the Passcode Settings page can be opened from the app.
+ (BOOL)isPasscodeSettingsAvailable;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PASSWORD_PASSWORD_SETTINGS_APP_INTERFACE_H_
