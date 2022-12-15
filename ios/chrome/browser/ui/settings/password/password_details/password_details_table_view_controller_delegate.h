// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_TABLE_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_TABLE_VIEW_CONTROLLER_DELEGATE_H_

@class PasswordDetails;
@class PasswordDetailsTableViewController;

@protocol PasswordDetailsTableViewControllerDelegate

// Called when user finished editing a password.
- (void)passwordDetailsViewController:
            (PasswordDetailsTableViewController*)viewController
               didEditPasswordDetails:(PasswordDetails*)password
                      withOldUsername:(NSString*)oldUsername
                       andOldPassword:(NSString*)oldPassword;

// Called when we finish treating all the passwords changes in the password
// details view.
- (void)didFinishEditingPasswordDetails;

// Called when user finished adding a new password credential.
- (void)passwordDetailsViewController:
            (PasswordDetailsTableViewController*)viewController
                didAddPasswordDetails:(NSString*)username
                             password:(NSString*)password;

// Called on every keystroke to check whether duplicates exist before adding a
// new credential.
- (void)checkForDuplicates:(NSString*)username;

// Called when an existing credential is to be displayed in the add credential
// flow.
- (void)showExistingCredential:(NSString*)username;

// Called when the user cancels the add password view.
- (void)didCancelAddPasswordDetails;

// Called every time the text in the website field is updated.
- (void)setWebsiteURL:(NSString*)website;

// Returns whether the website URL has http(s) scheme and is valid or not.
- (BOOL)isURLValid;

// Called to check if the url is missing the top-level domain.
- (BOOL)isTLDMissing;

// Checks if the username is reused for the same domain.
- (BOOL)isUsernameReused:(NSString*)newUsername forDomain:(NSString*)domain;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_TABLE_VIEW_CONTROLLER_DELEGATE_H_
