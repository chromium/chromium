// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_ISSUES_PRESENTER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_ISSUES_PRESENTER_H_

#include "components/password_manager/core/browser/ui/insecure_credentials_manager.h"

@protocol PasswordIssue;

// Presenter which handles commands from `PasswordsIssuesTableViewController`.
@protocol PasswordIssuesPresenter

// Called when view controller is removed.
- (void)dismissPasswordIssuesTableViewController;

// Called when Password Details screen should be shown.
- (void)presentPasswordIssueDetails:(PasswordIssue*)password;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_ISSUES_PRESENTER_H_
