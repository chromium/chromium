// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_PASSWORD_CHECKUP_METRICS_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_PASSWORD_CHECKUP_METRICS_H_

#import "ios/chrome/browser/passwords/model/password_checkup_utils.h"

namespace password_manager {

// Name of the histogram for logging count of unique pairs of username and
// password present in insecure credential warnings.
extern const char kInsecureCredentialsCountHistogram[];

// Name of the histogram for logging count of unique pairs of username and
// password present in insecure credentials warnings not muted by the user.
extern const char kUnmutedInsecureCredentialsCountHistogram[];

// Logs the user action of changing an insecure password on its affiliated
// website.
void LogChangePasswordOnWebsite(WarningType context);

// Logs the user action of editing an insecure password.
void LogEditPassword(WarningType context);

// Logs the user action of deleting an insecure password.
void LogDeletePassword(WarningType context);

// Logs the user action of revealing an insecure password.
void LogRevealPassword(WarningType context);

// Logs the user action of opening the list of password issues.
void LogOpenPasswordIssuesList(WarningType context);

// Logs the user action of muting a compromised credential warning.
void LogMuteCompromisedWarning();

// Logs the user action of unmuting a compromised credential warning.
void LogUnmuteCompromisedWarning();

// Logs the user action of starting a password check manually.
void LogStartPasswordCheckManually();

// Logs when a password check starts automatically.
void LogStartPasswordCheckAutomatically();

// Logs the user action of opening the password checkup home page.
void LogOpenPasswordCheckupHomePage();

// Logs the number of unique pairs of username and password present in an
// insecure credential warning.
void LogCountOfInsecureUsernamePasswordPairs(int count);

// Logs the number of unique pairs of username and password present in an
// insecure credential warning not muted by the user.
void LogCountOfUnmutedInsecureUsernamePasswordPairs(int count);

}  // namespace password_manager

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_PASSWORD_CHECKUP_METRICS_H_
