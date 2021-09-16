// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_ADD_PASSWORD_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_ADD_PASSWORD_MEDIATOR_DELEGATE_H_

namespace password_manager {
struct PasswordForm;
}  // namespace password_manager

// Delegate for AddPasswordMediator.
@protocol AddPasswordMediatorDelegate

// Called when the password details view controller is to be dismissed.
- (void)dismissPasswordDetailsTableViewController;

// Called when the alert is to be shown to the user for replacing an existing
// credential's password.
- (void)showReplacePasswordAlert:(NSString*)username hostUrl:(NSString*)hostUrl;

// Called after a new credential is added or an existing one is updated via the
// add credential flow.
- (void)setUpdatedPasswordForm:
    (const password_manager::PasswordForm&)passwordForm;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_ADD_PASSWORD_MEDIATOR_DELEGATE_H_
