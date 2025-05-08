// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_edit_table_view_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_details/password_details_consumer.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_controller_protocol.h"

@protocol PasswordDetailsHandler;
@protocol PasswordDetailsTableViewControllerDelegate;
@protocol ReauthenticationProtocol;

// Screen which shows password details and allows to edit it.
@interface PasswordDetailsTableViewController
    : AutofillEditTableViewController <PasswordDetailsConsumer,
                                       SettingsControllerProtocol,
                                       UIEditMenuInteractionDelegate>

// The designated initializer.
- (instancetype)init;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

// Handler for CredentialDetails related actions.
@property(nonatomic, weak) id<PasswordDetailsHandler> handler;

// Delegate for CredentialDetails related actions e.g. Password editing.
@property(nonatomic, weak) id<PasswordDetailsTableViewControllerDelegate>
    delegate;

// Module containing the reauthentication mechanism for interactions
// with password.
@property(nonatomic, weak) id<ReauthenticationProtocol> reauthModule;

// Called by coordinator when the user confirmed password editing from alert.
- (void)passwordEditingConfirmed;

// Shows the password details in edit mode without requiring any authentication.
- (void)showEditViewWithoutAuthentication;

// Brings back share button replaced with a spinner for the time when the
// necessary sharing info was being fetched.
- (void)showShareButton;

// Displays spinner next to the Edit/Done button.
- (void)showSpinnerOnRightNavigationBar;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_TABLE_VIEW_CONTROLLER_H_
