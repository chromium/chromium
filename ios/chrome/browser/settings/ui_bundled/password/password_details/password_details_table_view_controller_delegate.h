// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_TABLE_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_TABLE_VIEW_CONTROLLER_DELEGATE_H_

@class CredentialDetails;
@class PasswordDetailsTableViewController;

@protocol PasswordDetailsTableViewControllerDelegate

// Called when user finished editing a credential.
- (void)passwordDetailsViewController:
            (PasswordDetailsTableViewController*)viewController
             didEditCredentialDetails:(CredentialDetails*)credential
                      withOldUsername:(NSString*)oldUsername
                   oldUserDisplayName:(NSString*)oldUserDisplayName
                          oldPassword:(NSString*)oldPassword
                              oldNote:(NSString*)oldNote;

// Called when we finish treating all the passwords changes in the password
// details view.
- (void)didFinishEditingCredentialDetails;

// Checks if the username is reused for the same domain.
- (BOOL)isUsernameReused:(NSString*)newUsername forDomain:(NSString*)domain;

// Called by the view controller when the user wants to dismiss a compromised
// password warning.
- (void)dismissWarningForPassword:(CredentialDetails*)password;

// Called by the view controller when the user wants to restore a dismissed
// compromised password warning.
- (void)restoreWarningForCurrentPassword;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_TABLE_VIEW_CONTROLLER_DELEGATE_H_
