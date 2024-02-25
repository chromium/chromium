// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_SHARING_FIRST_RUN_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_SHARING_FIRST_RUN_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_first_run_action_handler.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"

// Bottom sheet displayed when the user opens password sharing flow for the
// first time and every consecutive time until they click "Share" button on it.
@interface PasswordSharingFirstRunViewController
    : ConfirmationAlertViewController

// The action handler for interactions in this view controller.
@property(nonatomic, weak) id<PasswordSharingFirstRunActionHandler>
    actionHandler;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_SHARING_FIRST_RUN_VIEW_CONTROLLER_H_
