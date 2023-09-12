// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SETTINGS_PASSWORD_SETTINGS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SETTINGS_PASSWORD_SETTINGS_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_delegate.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

// Delegate for the PasswordSettingsViewController to talk to its coordinator.
@protocol PasswordSettingsPresentationDelegate

// Method invoked when the user requests an export of their saved passwords.
- (void)startExportFlow;

// Method invoked when more information about a managed setting was requested.
// The `sourceView` button will be disabled and should be re-enabled once the
// requested info is dismissed.
- (void)showManagedPrefInfoForSourceView:(UIButton*)sourceView;

// Method invoked when the user has tapped on the "Passwords in Other Apps"
// menu item, requesting information about the state and usage of this feature.
- (void)showPasswordsInOtherAppsScreen;

// Method invoked when the user has requested to set up on-device encryption.
- (void)showOnDeviceEncryptionSetUp;

// Method invoked when the user has tapped "Learn More" about on-device
// encryption.
- (void)showOnDeviceEncryptionHelp;

@end

// ViewController used to present settings and infrequently-used actions
// relating to passwords. These are displayed in a submenu, separate from the
// Password Manager itself.
@interface PasswordSettingsViewController
    : SettingsRootTableViewController <PasswordSettingsConsumer>

// Delegate for communicating with the mediator.
@property(nonatomic, weak) id<PasswordSettingsDelegate> delegate;

// Delegate for communicating with the coordinator.
@property(nonatomic, weak) id<PasswordSettingsPresentationDelegate>
    presentationDelegate;

- (instancetype)init;

// Returns a rect suitable for anchoring the bulk move passwords to account
// alert.
- (CGRect)sourceRectForBulkMovePasswordsToAccount;

// Returns a rect suitable for anchoring alerts in the password export flow.
- (CGRect)sourceRectForPasswordExportAlerts;

// Returns a view suitable for anchoring alerts in the password manager
// settings.
- (UIView*)sourceViewForAlerts;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SETTINGS_PASSWORD_SETTINGS_VIEW_CONTROLLER_H_
