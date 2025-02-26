// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_MEDIATOR_DELEGATE_H_

// Delegate for the PasswordDetailsMediator.
@protocol PasswordDetailsMediatorDelegate

// Called when the user wants to dismiss a compromised credential warning.
- (void)showDismissWarningDialogWithCredentialDetails:
    (CredentialDetails*)password;

// Called when a credential has been updated or deleted. This will refresh the
// password suggestions list.
- (void)updateFormManagers;

// Called when sync status changes and the user is no longer eligible to
// complete a password sharing flow. This will stop password sharing
// coordinator or first run sharing coordinator if any of them is active.
- (void)stopPasswordSharingFlowIfActive;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_MEDIATOR_DELEGATE_H_
