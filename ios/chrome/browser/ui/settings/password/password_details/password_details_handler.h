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

// Called when the user wants to delete password. `origin` is a short website
// version. It is displayed inside dialog. If `origin` is nil dialog is
// displayed without message. `compromisedPassword` indicates whether password
// is compromised.
// TODO(crbug.com/1359392): Remove this when flag is cleaned up.
- (void)showPasswordDeleteDialogWithOrigin:(NSString*)origin
                       compromisedPassword:(BOOL)compromisedPassword;

// Called when the user wants to delete a password. `anchorView` and
// `anchorRect` should be the button that triggered this deletion flow and the
// coordinates of that button, to position the confirmation dialog correctly on
// tablets.
- (void)showPasswordDeleteDialogWithPasswordDetails:(PasswordDetails*)password
                                         anchorView:(UIView*)anchorView
                                         anchorRect:(CGRect)anchorRect;

// Called when the user wants to save edited password.
- (void)showPasswordEditDialogWithOrigin:(NSString*)origin;

// Called by the view controller when the user successfully copied a password.
- (void)onPasswordCopiedByUser;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_HANDLER_H_
