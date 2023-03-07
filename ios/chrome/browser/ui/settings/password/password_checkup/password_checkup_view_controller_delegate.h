// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_CHECKUP_PASSWORD_CHECKUP_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_CHECKUP_PASSWORD_CHECKUP_VIEW_CONTROLLER_DELEGATE_H_

// Delegate for `PasswordCheckupViewController`.
@protocol PasswordCheckupViewControllerDelegate

// Starts password check.
- (void)startPasswordCheck;

// Returns string containing the timestamp of the last password check. If the
// check finished less than 1 minute ago string will look "Last check just
// now.", otherwise "Last check X minutes/hours... ago.". If check never run,
// string will be "Check never run.".
- (NSString*)formattedElapsedTimeSinceLastCheck;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_CHECKUP_PASSWORD_CHECKUP_VIEW_CONTROLLER_DELEGATE_H_
