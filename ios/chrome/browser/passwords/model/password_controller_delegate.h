// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_PASSWORD_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_PASSWORD_CONTROLLER_DELEGATE_H_

namespace password_manager {
struct CredentialUIEntry;
}  // namespace password_manager

// Delegate for registering view controller and displaying its view. Used to
// add views to BVC.
// TODO(crbug.com/40806286): Refactor this API to not be coupled to the BVC and
// to use the UI command patterns.
@protocol PasswordControllerDelegate

// Adds `viewController` as child controller in order to display auto sign-in
// notification. Returns YES if view was displayed, NO otherwise.
- (BOOL)displaySignInNotification:(UIViewController*)viewController
                        fromTabId:(NSString*)tabId;

// Opens the list of saved passwords in the settings.
- (void)displaySavedPasswordList;

// Opens the password details for credential.
- (void)showPasswordDetailsForCredential:
    (password_manager::CredentialUIEntry)credential;

@end

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_PASSWORD_CONTROLLER_DELEGATE_H_
