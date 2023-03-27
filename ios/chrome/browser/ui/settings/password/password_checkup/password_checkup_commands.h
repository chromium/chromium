// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_CHECKUP_PASSWORD_CHECKUP_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_CHECKUP_PASSWORD_CHECKUP_COMMANDS_H_

namespace password_manager {
enum class WarningType;
}

// Commands relative to the Password Checkup homepage.
@protocol PasswordCheckupCommands

// Called when view controller is removed.
- (void)dismissPasswordCheckupViewController;

// Opens the Password Issues list displaying compromised, weak or reused
// credentials.
- (void)showPasswordIssuesWithWarningType:
    (password_manager::WarningType)warningType;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_CHECKUP_PASSWORD_CHECKUP_COMMANDS_H_
