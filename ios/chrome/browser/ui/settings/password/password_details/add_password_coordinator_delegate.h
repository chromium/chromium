// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_ADD_PASSWORD_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_ADD_PASSWORD_COORDINATOR_DELEGATE_H_

#import "ios/chrome/browser/ui/settings/password/reauthentication/password_manager_reauthentication_delegate.h"

namespace password_manager {
struct CredentialUIEntry;
}  // namespace password_manager

// Delegate for AddPasswordCoordinator.
@protocol
    AddPasswordCoordinatorDelegate <PasswordManagerReauthenticationDelegate>

// Called when the add view controller is to removed.
- (void)passwordDetailsTableViewControllerDidFinish:
    (AddPasswordCoordinator*)coordinator;

// Called after a new credential is added or an existing one is updated via the
// add credential flow.
- (void)setMostRecentlyUpdatedPasswordDetails:
    (const password_manager::CredentialUIEntry&)credential;

// Called when the user clicks on the "View Password" in the section alert. The
// section alert is shown when there exists an existing credential with the same
// username/website combination as that of the credential being added manually.
// Would stop the add password coordinator and dismiss the view controller.
- (void)dismissAddViewControllerAndShowPasswordDetails:
            (const password_manager::CredentialUIEntry&)credential
                                           coordinator:(AddPasswordCoordinator*)
                                                           coordinator;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_ADD_PASSWORD_COORDINATOR_DELEGATE_H_
