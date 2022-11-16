// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_ADD_PASSWORD_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_ADD_PASSWORD_MEDIATOR_DELEGATE_H_

namespace password_manager {
struct CredentialUIEntry;
}  // namespace password_manager

// Delegate for AddPasswordMediator.
@protocol AddPasswordMediatorDelegate

// Called when the add password view controller is to be dismissed.
- (void)dismissAddPasswordTableViewController;

// Called after a new credential is added or an existing one is updated via the
// add credential flow.
- (void)setUpdatedPassword:
    (const password_manager::CredentialUIEntry&)credential;

// Called when the "View Password" is tapped in the section alert. The section
// alert is shown when there exists an existing credential with the same
// username/website combination as that of the credential being added manually.
- (void)showPasswordDetailsControllerWithCredential:
    (const password_manager::CredentialUIEntry&)credential;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_ADD_PASSWORD_MEDIATOR_DELEGATE_H_
