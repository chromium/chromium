// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_ACCOUNTS_MANAGE_ACCOUNTS_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_ACCOUNTS_MANAGE_ACCOUNTS_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/manage_accounts_consumer.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/with_overridable_model_identity_data_source.h"
#import "ios/chrome/browser/ui/settings/settings_controller_protocol.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

@protocol ManageAccountsModelIdentityDataSource;
@protocol ManageAccountsMutator;
@protocol SettingsCommands;

// TableView that handles the settings for accounts when the user is signed in
// to Chrome.
@interface ManageAccountsTableViewController
    : SettingsRootTableViewController <ManageAccountsConsumer,
                                       SettingsControllerProtocol,
                                       WithOverridableModelIdentityDataSource>

// Model delegate.
@property(nonatomic, weak) id<ManageAccountsModelIdentityDataSource>
    modelIdentityDataSource;

// Mutator.
@property(nonatomic, weak) id<ManageAccountsMutator> mutator;

- (instancetype)initWithOfferSignout:(BOOL)offerSignout
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_ACCOUNTS_MANAGE_ACCOUNTS_TABLE_VIEW_CONTROLLER_H_
