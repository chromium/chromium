// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PASSWORD_PASSWORD_DETAILS_ADD_PASSWORD_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PASSWORD_PASSWORD_DETAILS_ADD_PASSWORD_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_edit_table_view_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_details/add_password_details_consumer.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_details/password_details_consumer.h"

@protocol PasswordDetailsHandler;
@protocol AddPasswordViewControllerDelegate;
@protocol ReauthenticationProtocol;

// Screen which allows the user to save a new password.
@interface AddPasswordViewController
    : AutofillEditTableViewController <AddPasswordDetailsConsumer>

// The designated initializer.
- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

// Delegate for CredentialDetails related actions e.g. Password editing.
@property(nonatomic, weak) id<AddPasswordViewControllerDelegate> delegate;

// Module containing the reauthentication mechanism for interactions
// with password.
@property(nonatomic, weak) id<ReauthenticationProtocol> reauthModule;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PASSWORD_PASSWORD_DETAILS_ADD_PASSWORD_VIEW_CONTROLLER_H_
