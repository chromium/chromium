// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SECURITY_ALERT_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SECURITY_ALERT_COMMANDS_H_

@protocol SecurityAlertCommands <NSObject>

// Presents an alert with the passed body. And a title indicating something is
// not secure. Credit card numbers and passwords cannot be filled over HTTP, and
// passwords can only be filled in a password field; when a user attempts to
// autofill these a warning is displayed using the security alert presenter
- (void)presentSecurityWarningAlertWithText:(NSString*)body;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SECURITY_ALERT_COMMANDS_H_
