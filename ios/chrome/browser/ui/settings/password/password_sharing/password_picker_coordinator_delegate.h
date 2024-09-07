// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_PICKER_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_PICKER_COORDINATOR_DELEGATE_H_

namespace password_manager {
struct CredentialUIEntry;
}

@class PasswordPickerCoordinator;

// Delegate for PasswordPickerCoordinator.
@protocol PasswordPickerCoordinatorDelegate

// Called when the user cancels or dismisses the password selection.
- (void)passwordPickerCoordinatorWasDismissed:
    (PasswordPickerCoordinator*)coordinator;

// Called when the user confirms the selection by clicking the next button.
- (void)passwordPickerWithNavigationController:
            (UINavigationController*)navigationController
                           didSelectCredential:
                               (const password_manager::CredentialUIEntry&)
                                   credential;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_PICKER_COORDINATOR_DELEGATE_H_
