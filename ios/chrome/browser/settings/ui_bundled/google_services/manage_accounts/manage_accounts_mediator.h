// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_GOOGLE_SERVICES_MANAGE_ACCOUNTS_MANAGE_ACCOUNTS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_GOOGLE_SERVICES_MANAGE_ACCOUNTS_MANAGE_ACCOUNTS_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/settings/ui_bundled/google_services/manage_accounts/manage_accounts_mediator_delegate.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/manage_accounts/manage_accounts_model_identity_data_source.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/manage_accounts/manage_accounts_mutator.h"

class AuthenticationService;
class ChromeAccountManagerService;
namespace signin {
class IdentityManager;
}  // namespace signin
@protocol ManageAccountsConsumer;

// Mediator for the Accounts TableView Controller.
@interface ManageAccountsMediator
    : NSObject <ManageAccountsModelIdentityDataSource, ManageAccountsMutator>

// Consumer.
@property(nonatomic, weak) id<ManageAccountsConsumer> consumer;

// Delegate.
@property(nonatomic, weak) id<ManageAccountsMediatorDelegate> delegate;

// Designated initializer.
- (instancetype)
    initWithAccountManagerService:
        (ChromeAccountManagerService*)accountManagerService
                      authService:(AuthenticationService*)authService
                  identityManager:(signin::IdentityManager*)identityManager
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator to all observers and services.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_GOOGLE_SERVICES_MANAGE_ACCOUNTS_MANAGE_ACCOUNTS_MEDIATOR_H_
