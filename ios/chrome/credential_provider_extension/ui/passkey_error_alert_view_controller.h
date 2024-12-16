// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_PASSKEY_ERROR_ALERT_VIEW_CONTROLLER_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_PASSKEY_ERROR_ALERT_VIEW_CONTROLLER_H_

#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"

// Possible errors for showing the PasskeyErrorAlertViewController.
enum class ErrorType {
  // Saving credentials has been disabled by enterprise policy.
  kEnterpriseDisabledSavingCredentials,

  // User is signed out of Chrome.
  kSignedOut,

  // Saving credentials to account has been disabled by the user.
  kUserDisabledSavingCredentialsInPasswordSettings,

  // Saving credentials has been disabled by the user in Password Settings.
  kUserDisabledSavingCredentialsToAccount,
};

// Displays a view informing the user of why it's not possible to proceed with
// the current passkey credential request at the moment, and the steps that they
// can take to resolve the issue (if any).
@interface PasskeyErrorAlertViewController : ConfirmationAlertViewController

// Designated initializer. `errorType` indicates the error for which the passkey
// error alert screen needs to be shown, which impacts the screen's content.
- (instancetype)initForErrorType:(ErrorType)errorType NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_PASSKEY_ERROR_ALERT_VIEW_CONTROLLER_H_
