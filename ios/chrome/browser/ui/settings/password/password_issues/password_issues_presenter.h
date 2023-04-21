// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_ISSUES_PASSWORD_ISSUES_PRESENTER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_ISSUES_PASSWORD_ISSUES_PRESENTER_H_

#include "components/password_manager/core/browser/ui/insecure_credentials_manager.h"

@protocol PasswordIssue;
@class CrURL;

// Presenter which handles commands from `PasswordsIssuesTableViewController`.
@protocol PasswordIssuesPresenter

// Called when view controller is removed.
- (void)dismissPasswordIssuesTableViewController;

- (void)dismissAndOpenURL:(CrURL*)URL;

// Called when Password Details screen should be shown.
- (void)presentPasswordIssueDetails:(PasswordIssue*)password;

// Called when password issues should be shown for dismissed compromised
// credentials.
- (void)presentDismissedCompromisedCredentials;

// Called when the user removed all issues and/or dismissed warnings currently
// displayed in PasswordsIssuesTableViewController.
// Password Issues and dismissed warnings can be deleted or resolved by the user
// from the Password Details screen. If the user deletes or resolves the only
// remaining issue in Password Issues, we dismiss it as there is no content to
// display in the page. Calling this method dismisses Password Issues and any
// view controller presented by a child coordinator.
- (void)dismissAfterAllIssuesGone;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_ISSUES_PASSWORD_ISSUES_PRESENTER_H_
