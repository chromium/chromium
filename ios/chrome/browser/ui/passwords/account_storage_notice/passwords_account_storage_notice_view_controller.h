// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PASSWORDS_ACCOUNT_STORAGE_NOTICE_PASSWORDS_ACCOUNT_STORAGE_NOTICE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_PASSWORDS_ACCOUNT_STORAGE_NOTICE_PASSWORDS_ACCOUNT_STORAGE_NOTICE_VIEW_CONTROLLER_H_

#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"

#import <Foundation/Foundation.h>

// Handles user interactions with PasswordsAccountStorageNoticeViewController.
@protocol
    PasswordsAccountStorageNoticeActionHandler <ConfirmationAlertActionHandler>

// The link to password settings was tapped.
- (void)confirmationAlertSettingsAction;

// The sheet was dismissed by sliding.
- (void)confirmationAlertSwipeDismissAction;

@end

// Bottom sheet that notifies the user they are now saving passwords to their
// Google Account. The sheet contains:
// - An OK button, in case the user wants to keep the feature enabled.
// - A link to the password settings page, in case the user wants to disable
// the feature via the appropriate switch.
@interface PasswordsAccountStorageNoticeViewController
    : ConfirmationAlertViewController

- (instancetype)initWithActionHandler:
    (id<PasswordsAccountStorageNoticeActionHandler>)actionHandler
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)name
                         bundle:(NSBundle*)bundle NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

// The action handler for interactions in this View Controller.
@property(nonatomic, weak) id<PasswordsAccountStorageNoticeActionHandler>
    actionHandler;

@end

#endif  // IOS_CHROME_BROWSER_UI_PASSWORDS_ACCOUNT_STORAGE_NOTICE_PASSWORDS_ACCOUNT_STORAGE_NOTICE_VIEW_CONTROLLER_H_
