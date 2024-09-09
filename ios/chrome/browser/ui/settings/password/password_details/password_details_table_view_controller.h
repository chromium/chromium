// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/autofill/autofill_edit_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_consumer.h"
#import "ios/chrome/browser/ui/settings/settings_controller_protocol.h"

@protocol ApplicationCommands;
@protocol PasswordDetailsHandler;
@protocol PasswordDetailsTableViewControllerDelegate;
@protocol ReauthenticationProtocol;
@protocol SnackbarCommands;

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

// ApplicationCommands handler.
@property(nonatomic, weak) id<ApplicationCommands> applicationCommandsHandler;

// SnackbarCommands handler.
@property(nonatomic, weak) id<SnackbarCommands> snackbarCommandsHandler;

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

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_TABLE_VIEW_CONTROLLER_H_
