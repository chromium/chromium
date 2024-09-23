// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_MEDIATOR_DELEGATE_H_

// Delegate for the PasswordDetailsMediator.
@protocol PasswordDetailsMediatorDelegate

// Called when the user wants to dismiss a compromised credential warning.
- (void)showDismissWarningDialogWithCredentialDetails:
    (CredentialDetails*)password;

// Called when a credential has been updated or deleted. This will refresh the
// password suggestions list.
- (void)updateFormManagers;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_MEDIATOR_DELEGATE_H_
