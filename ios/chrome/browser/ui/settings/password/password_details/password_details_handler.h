// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_HANDLER_H_

@class PasswordDetails;

// Presenter which handles commands from `PasswordDetailsViewController`.
@protocol PasswordDetailsHandler

// Called when the view controller was dismissed.
- (void)passwordDetailsTableViewControllerDidDisappear;

// Shows a dialog offering the user to set a passcode in order to see the
// password.
- (void)showPasscodeDialog;

// Called when the user wants to delete a password. `anchorView` should be
// the button that triggered this deletion flow, to position the confirmation
// dialog correctly on tablets.
- (void)showPasswordDeleteDialogWithPasswordDetails:(PasswordDetails*)password
                                         anchorView:(UIView*)anchorView;

// Called when the user wants to move a password from profile store to account
// store.
- (void)moveCredentialToAccountStore:(PasswordDetails*)password;

// Called when the user wants to save edited password.
- (void)showPasswordEditDialogWithOrigin:(NSString*)origin;

// Called by the view controller when the user successfully copied a password.
- (void)onPasswordCopiedByUser;

// Called when all passwords were deleted, in order to close the view.
- (void)onAllPasswordsDeleted;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_HANDLER_H_
