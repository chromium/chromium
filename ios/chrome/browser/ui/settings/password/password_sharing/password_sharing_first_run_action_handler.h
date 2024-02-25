// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_SHARING_FIRST_RUN_ACTION_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_SHARING_FIRST_RUN_ACTION_HANDLER_H_

#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

// Handles user interactions in the password sharing first run view.
@protocol PasswordSharingFirstRunActionHandler <ConfirmationAlertActionHandler>

// Handles taps on the link to learn more about password sharing.
- (void)learnMoreLinkWasTapped;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_SHARING_FIRST_RUN_ACTION_HANDLER_H_
