// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_CHECKUP_PASSWORD_CHECKUP_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_CHECKUP_PASSWORD_CHECKUP_VIEW_CONTROLLER_DELEGATE_H_

// Delegate for `PasswordCheckupViewController`.
@protocol PasswordCheckupViewControllerDelegate

// Starts password check.
- (void)startPasswordCheck;

// Toggles Safety Check notifications on/off. The coordinator should present
// appropriate UI (e.g., notification opt-in/opt-out) to the user
// based on the current notification state.
- (void)toggleSafetyCheckNotifications;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_CHECKUP_PASSWORD_CHECKUP_VIEW_CONTROLLER_DELEGATE_H_
