// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/password/passwords_consumer.h"
#import "ios/chrome/browser/ui/settings/settings_controller_protocol.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_favicon_data_source.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"

class Browser;
@class PasswordExporter;
@protocol PasswordsSettingsCommands;
@protocol PasswordsTableViewControllerDelegate;
@protocol PasswordsTableViewControllerPresentationDelegate;

@interface PasswordsTableViewController
    : SettingsRootTableViewController <PasswordsConsumer,
                                       SettingsControllerProtocol>

// The designated initializer. `browser` must not be nil.
- (instancetype)initWithBrowser:(Browser*)browser NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

// Stores the most recently updated credential.
- (void)setMostRecentlyUpdatedPasswordDetails:
    (const password_manager::PasswordForm&)password;

@property(nonatomic, weak) id<PasswordsSettingsCommands> handler;

// Delegate.
@property(nonatomic, weak) id<PasswordsTableViewControllerDelegate> delegate;

@property(nonatomic, weak) id<PasswordsTableViewControllerPresentationDelegate>
    presentationDelegate;

// Reauthentication module.
@property(nonatomic, strong) id<ReauthenticationProtocol>
    reauthenticationModule;

// Data source for favicon images.
@property(nonatomic, weak) id<TableViewFaviconDataSource> imageDataSource;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_TABLE_VIEW_CONTROLLER_H_
