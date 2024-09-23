// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_HANDLER_H_

@class CredentialDetails;

// Presenter which handles commands from `PasswordDetailsViewController`.
@protocol PasswordDetailsHandler

// Called when the view controller was dismissed by tapping on the Back button
// of the view controller. This is called by the  -viewDidDisappear method of
// the Password Details View Controller.
- (void)passwordDetailsTableViewControllerWasDismissed;

// Called to dismiss the view controller. Only call this method when the view
// controller is displayed outside the Settings context and the user taps on the
// cancel button. In the case of the Settings context, it is handled by the
// -passwordDetailsTableViewControllerWasDismissed method above that is called
// in -viewDidDisappear.
- (void)dismissPasswordDetailsTableViewController;

// Called when the user wants to delete a password. `anchorView` should be
// the button that triggered this deletion flow, to position the confirmation
// dialog correctly on tablets.
// TODO(crbug.com/40880506): CredentialDetails is a concept that should only be
// consumed by the view controller, it doesn't belong in this protocol.
// Ultimately this is passed to map to a CredentialUIEntry. There should be a
// better way to map. Either pass (username, password, sign-on realm), which are
// the identifiers being used now, or something like sort key.
- (void)showCredentialDeleteDialogWithCredentialDetails:
            (CredentialDetails*)credential
                                             anchorView:(UIView*)anchorView;

// Called when the user wants to move a password from profile store to account
// store.`anchorView` should be the button that triggered this move flow, to
// position the confirmation dialog correctly on tablets. This will trigger an
// extra confirmation step in case there is a conflicting credential in the
// account store (same username but different password value). `movedCompletion`
// is called if the move is performed successfully.
- (void)moveCredentialToAccountStore:(CredentialDetails*)credential
                          anchorView:(UIView*)anchorView
                     movedCompletion:(void (^)())movedCompletion;

// Called when the user wants to save edited password.
- (void)showPasswordEditDialogWithOrigin:(NSString*)origin;

// Called when all passwords were deleted, in order to close the view.
- (void)onAllPasswordsDeleted;

// Called when share button is pressed by the user.
- (void)onShareButtonPressed;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_HANDLER_H_
