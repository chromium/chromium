// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_PICKER_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_PICKER_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_

namespace password_manager {
struct CredentialUIEntry;
}

@class PasswordPickerViewController;

// Delegate for PasswordPickerViewController.
@protocol PasswordPickerViewControllerPresentationDelegate <NSObject>

// Called when the user clicks cancel button or dismisses the view by swiping.
- (void)passwordPickerWasDismissed:(PasswordPickerViewController*)controller;

// Called when the user clicks next button with selected credential.
- (void)passwordPickerClosed:(PasswordPickerViewController*)controller
      withSelectedCredential:
          (const password_manager::CredentialUIEntry&)credential;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_PICKER_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_
