// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SETTINGS_PASSWORD_BULK_MOVE_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SETTINGS_PASSWORD_BULK_MOVE_HANDLER_H_

// Protocol which handles showing alerts or authentication in response to user
// interactions with the bulk move passwords to account section in password
// manager settings.
@protocol BulkMoveLocalPasswordsToAccountHandler

// TODO(crbug.com/1479177): Add auth for bulk move passwords.
- (void)showAuthentication;

// Show the move passwords to account confirmation alert with constructed title
// and description.
- (void)showConfirmationDialogWithAlertTitle:(NSString*)alertTitle
                            alertDescription:(NSString*)alertDescription;

// TODO(crbug.com/1479177): Add auth for bulk move passwords.
- (void)showSetPasscodeAlert;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SETTINGS_PASSWORD_BULK_MOVE_HANDLER_H_
