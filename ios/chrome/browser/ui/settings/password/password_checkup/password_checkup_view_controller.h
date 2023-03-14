// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_CHECKUP_PASSWORD_CHECKUP_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_CHECKUP_PASSWORD_CHECKUP_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_consumer.h"
#import "ios/chrome/browser/ui/settings/settings_controller_protocol.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

@protocol PasswordCheckupCommands;
@protocol PasswordCheckupViewControllerDelegate;

// Screen that presents the Password Checkup homepage.
@interface PasswordCheckupViewController
    : SettingsRootTableViewController <PasswordCheckupConsumer,
                                       SettingsControllerProtocol>

// Handler for PasswordCheckupCommands.
@property(nonatomic, weak) id<PasswordCheckupCommands> handler;

// Delegate.
@property(nonatomic, weak) id<PasswordCheckupViewControllerDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_CHECKUP_PASSWORD_CHECKUP_VIEW_CONTROLLER_H_
