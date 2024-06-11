// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_ADD_PASSWORD_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_ADD_PASSWORD_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/autofill/autofill_edit_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/password_details/add_password_details_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_consumer.h"

@protocol ApplicationCommands;
@protocol PasswordDetailsHandler;
@protocol AddPasswordViewControllerDelegate;
@protocol ReauthenticationProtocol;
@protocol SnackbarCommands;

// Screen which allows the user to save a new password.
@interface AddPasswordViewController
    : AutofillEditTableViewController <AddPasswordDetailsConsumer>

// The designated initializer.
- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

// Delegate for CredentialDetails related actions e.g. Password editing.
@property(nonatomic, weak) id<AddPasswordViewControllerDelegate> delegate;

// ApplicationCommands handler.
@property(nonatomic, weak) id<ApplicationCommands> applicationCommandsHandler;

// SnackbarCommands handler.
@property(nonatomic, weak) id<SnackbarCommands> snackbarCommandsHandler;

// Module containing the reauthentication mechanism for interactions
// with password.
@property(nonatomic, weak) id<ReauthenticationProtocol> reauthModule;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_ADD_PASSWORD_VIEW_CONTROLLER_H_
