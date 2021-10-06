// Copyright 2020 The Chromium Authors. All rights reserved.
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
               didEditPasswordDetails:(PasswordDetails*)password;

// Called when user finished adding a new password credential.
- (void)passwordDetailsViewController:
            (PasswordDetailsTableViewController*)viewController
        didAddPasswordDetailsWithSite:(NSString*)website
                             username:(NSString*)username
                             password:(NSString*)password;

// Called on every keystroke to check whether duplicates exist before adding a
// new credential.
- (void)checkForDuplicatesWithSite:(NSString*)website
                          username:(NSString*)username;

// Called when the user cancels the add password view.
- (void)didCancelAddPasswordDetails;

// Called when the user is validated and confirmed to replace the existing
// credential from the add password view.
- (void)didConfirmReplaceExistingCredential;

// Checks if the username is reused for the same domain.
- (BOOL)isUsernameReused:(NSString*)newUsername;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_TABLE_VIEW_CONTROLLER_DELEGATE_H_
