// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SETTINGS_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SETTINGS_APP_INTERFACE_H_

#import <UIKit/UIKit.h>

#import "components/password_manager/core/browser/bulk_leak_check_service_interface.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"

// EarlGreyScopedBlockSwizzlerAppInterface contains the app-side
// implementation for helpers. These helpers are compiled into
// the app binary and can be called from either app or test code.
@interface PasswordSettingsAppInterface : NSObject

// Sets a re-authentication mock (i.e. what asks user for fingerprint to
// view password) and its options for next test.
+ (void)setUpMockReauthenticationModule;
+ (void)setUpMockReauthenticationModuleForAddPassword;
+ (void)setUpMockReauthenticationModuleForPasswordManager;
+ (void)mockReauthenticationModuleExpectedResult:
    (ReauthenticationResult)expectedResult;
+ (void)mockReauthenticationModuleCanAttempt:(BOOL)canAttempt;

// Similar to the methods above, but with a companion to remove the override.
+ (void)setUpMockReauthenticationModuleForExportFromSettings;
+ (void)removeMockReauthenticationModuleForExportFromSettings;

// Dismisses snack bar.  Used before next test.
+ (void)dismissSnackBar;

// Removes all credentials stored.
+ (BOOL)clearPasswordStore;

// Creates multiple password form with index being part of the username,
// password, origin and realm.
+ (void)saveExamplePasswordWithCount:(NSInteger)count;

// Creates password form for given fields.
+ (BOOL)saveExamplePassword:(NSString*)password
                   username:(NSString*)username
                     origin:(NSString*)origin;

// Creates password form for given fields.
+ (BOOL)saveExampleNote:(NSString*)note
               password:(NSString*)password
               username:(NSString*)username
                 origin:(NSString*)origin;

// Creates a compromised password form.
+ (BOOL)saveCompromisedPassword:(NSString*)password
                       username:(NSString*)username
                         origin:(NSString*)origin;

// Creates a muted compromised password form.
+ (BOOL)saveMutedCompromisedPassword:(NSString*)password
                            username:(NSString*)userName
                              origin:(NSString*)origin;

// Creates a blocked password form for given origin.
+ (BOOL)saveExampleBlockedOrigin:(NSString*)origin;

// Creates a federated password form for given origins and user.
+ (BOOL)saveExampleFederatedOrigin:(NSString*)federatedOrigin
                          username:(NSString*)username
                            origin:(NSString*)origin;

// Gets number of password form stored.
+ (NSInteger)passwordStoreResultsCount;

// Returns YES if credential service is enabled.
+ (BOOL)isCredentialsServiceEnabled;

// Replaces the BrowserState's BulkLeakCheckService with a fake one.
+ (void)setupFakeBulkLeakCheckService;

// Sets the FakeBulkLeakCheck's buffered state.
+ (void)setFakeBulkLeakCheckBufferedState:
    (password_manager::BulkLeakCheckServiceInterface::State)state;

// Returns true if the Password Checkup feature flag is enabled.
+ (BOOL)isPasswordCheckupEnabled;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SETTINGS_APP_INTERFACE_H_
