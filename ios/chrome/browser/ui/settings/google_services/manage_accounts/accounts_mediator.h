// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_ACCOUNTS_ACCOUNTS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_ACCOUNTS_ACCOUNTS_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/accounts_mediator_delegate.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/accounts_model_identity_data_source.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/accounts_mutator.h"

@protocol AccountsConsumer;
class AuthenticationService;
class ChromeAccountManagerService;
namespace signin {
class IdentityManager;
}  // namespace signin
namespace syncer {
class SyncService;
}  // namespace syncer

// Mediator for the Accounts TableView Controller.
@interface AccountsMediator
    : NSObject <AccountsModelIdentityDataSource, AccountsMutator>

// Consumer.
@property(nonatomic, weak) id<AccountsConsumer> consumer;

// Delegate.
@property(nonatomic, weak) id<AccountsMediatorDelegate> delegate;

// Designated initializer.
- (instancetype)initWithSyncService:(syncer::SyncService*)syncService
              accountManagerService:
                  (ChromeAccountManagerService*)accountManagerService
                        authService:(AuthenticationService*)authService
                    identityManager:(signin::IdentityManager*)identityManager
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator to all observers and services.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_ACCOUNTS_ACCOUNTS_MEDIATOR_H_
