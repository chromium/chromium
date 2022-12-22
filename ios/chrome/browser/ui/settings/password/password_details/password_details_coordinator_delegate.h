// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_COORDINATOR_DELEGATE_H_

namespace password_manager {
struct CredentialUIEntry;
}  // namespace password_manager

@class PasswordDetailsCoordinator;

// Delegate for PasswordIssuesCoordinator.
@protocol PasswordDetailsCoordinatorDelegate

// Called when the view controller was removed from navigation controller.
- (void)passwordDetailsCoordinatorDidRemove:
    (PasswordDetailsCoordinator*)coordinator;

// Called when user deleted password. This action should be handled outside to
// update the list of passwords immediately. Callers should pass YES for
// `shouldDismiss` if this is the last password on the page, to ensure the view
// controller gets dismissed.
- (void)passwordDetailsCoordinator:(PasswordDetailsCoordinator*)coordinator
                  deleteCredential:
                      (const password_manager::CredentialUIEntry&)credential
                 shouldDismissView:(BOOL)shouldDismiss;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_COORDINATOR_DELEGATE_H_
