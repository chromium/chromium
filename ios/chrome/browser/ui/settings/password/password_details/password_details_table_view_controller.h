// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/autofill/autofill_edit_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/password_details/add_password_details_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_consumer.h"

@protocol ApplicationCommands;
@protocol AddPasswordHandler;
@protocol PasswordDetailsHandler;
@protocol PasswordDetailsTableViewControllerDelegate;
@protocol ReauthenticationProtocol;

// Denotes the credential type that is being displayed by the view controller.
typedef NS_ENUM(NSInteger, CredentialType) {
  CredentialTypeRegular = kItemTypeEnumZero,
  CredentialTypeBlocked,
  CredentialTypeFederation,
  CredentialTypeNew,
};

// Screen which shows password details and allows to edit it.
@interface PasswordDetailsTableViewController
    : AutofillEditTableViewController <AddPasswordDetailsConsumer,
                                       PasswordDetailsConsumer>

// The designated initializer.
// `syncingUserEmail` stores the user email if the user is authenticated amd
// syncing passwords.
- (instancetype)initWithCredentialType:(CredentialType)credentialType
                      syncingUserEmail:(NSString*)syncingUserEmail
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

// Handler for PasswordDetails related actions.
@property(nonatomic, weak) id<PasswordDetailsHandler> handler;

// Handler for AddPasswordDetails related actions.
@property(nonatomic, weak) id<AddPasswordHandler> addPasswordHandler;

// Delegate for PasswordDetails related actions e.g. Password editing.
@property(nonatomic, weak) id<PasswordDetailsTableViewControllerDelegate>
    delegate;

// Dispatcher for this ViewController.
// TODO(crbug.com/1323778): This class needs to have an explicit
// id<SnacbarCommands> handler property instead of using BrowserCommands. There
// will also be an explicit (separate) ApplicationCommands handler.
@property(nonatomic, weak) id<ApplicationCommands, BrowserCommands>
    commandsHandler;

// Module containing the reauthentication mechanism for interactions
// with password.
@property(nonatomic, weak) id<ReauthenticationProtocol> reauthModule;

// Called by coordinator when the user confirmed password editing from alert.
- (void)passwordEditingConfirmed;

// Shows the password details in edit mode without requiring any authentication.
- (void)showEditViewWithoutAuthentication;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_TABLE_VIEW_CONTROLLER_H_
