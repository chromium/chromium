// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SETTINGS_PASSWORD_BULK_MOVE_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SETTINGS_PASSWORD_BULK_MOVE_HANDLER_H_

// Protocol which handles showing alerts or authentication in response to user
// interactions with the bulk move passwords to account section in password
// manager settings.
@protocol BulkMoveLocalPasswordsToAccountHandler

// Attempts to auth the user and moves the passwords if auth is successful. If
// it's not successful, does nothing. If the user has no auth set on their
// device, prompt them to add some.
- (void)showAuthenticationForMovePasswordsToAccountWithMessage:
    (NSString*)message;

// Show the move passwords to account confirmation alert with constructed title
// and description.
- (void)showConfirmationDialogWithAlertTitle:(NSString*)alertTitle
                            alertDescription:(NSString*)alertDescription;

// Shows the snackbar confirming to the user that their local passwords have
// been saved to their account.
- (void)showMovedToAccountSnackbarWithPasswordCount:(int)count
                                          userEmail:(std::string)email;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SETTINGS_PASSWORD_BULK_MOVE_HANDLER_H_
