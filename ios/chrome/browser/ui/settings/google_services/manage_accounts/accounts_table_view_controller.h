// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_ACCOUNTS_ACCOUNTS_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_ACCOUNTS_ACCOUNTS_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/accounts_consumer.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/with_overridable_model_identity_data_source.h"
#import "ios/chrome/browser/ui/settings/settings_controller_protocol.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

@protocol AccountsModelIdentityDataSource;
@protocol AccountsMutator;
@protocol SettingsCommands;

// TableView that handles the settings for accounts when the user is signed in
// to Chrome.
@interface AccountsTableViewController
    : SettingsRootTableViewController <AccountsConsumer,
                                       SettingsControllerProtocol,
                                       WithOverridableModelIdentityDataSource>

// Model delegate.
@property(nonatomic, weak) id<AccountsModelIdentityDataSource>
    modelIdentityDataSource;

// Mutator.
@property(nonatomic, weak) id<AccountsMutator> mutator;

- (instancetype)initWithOfferSignout:(BOOL)offerSignout
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_ACCOUNTS_ACCOUNTS_TABLE_VIEW_CONTROLLER_H_
