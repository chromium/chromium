// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_ACCOUNT_MENU_ACCOUNT_MENU_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_ACCOUNT_MENU_ACCOUNT_MENU_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"
#import "ios/chrome/browser/authentication/ui_bundled/account_menu/account_menu_data_source.h"
#import "ios/chrome/browser/authentication/ui_bundled/account_menu/account_menu_mutator.h"

enum class AccountMenuAccessPoint;
@protocol AccountMenuConsumer;
@protocol AccountMenuMediatorDelegate;
@protocol SyncErrorSettingsCommandHandler;
class AuthenticationService;
class ChromeAccountManagerService;
class GURL;
class PrefService;
namespace signin {
class IdentityManager;
}  // namespace signin
namespace syncer {
class SyncService;
}  // namespace syncer

// Mediator for AccountMenu
@interface AccountMenuMediator
    : NSObject <AccountMenuDataSource, AccountMenuMutator>

// The consumer of the mediator.
@property(nonatomic, weak) id<AccountMenuConsumer> consumer;

// The delegate of the mediator.
@property(nonatomic, weak) id<AccountMenuMediatorDelegate> delegate;

// The sync error settings command handler.
@property(nonatomic, weak) id<SyncErrorSettingsCommandHandler>
    syncErrorSettingsCommandHandler;

- (instancetype)initWithSyncService:(syncer::SyncService*)syncService
              accountManagerService:
                  (ChromeAccountManagerService*)accountManagerService
                        authService:(AuthenticationService*)authService
                    identityManager:(signin::IdentityManager*)identityManager
                              prefs:(PrefService*)prefs
                        accessPoint:(AccountMenuAccessPoint)accessPoint
                                URL:(const GURL&)url
               prepareChangeProfile:(ProceduralBlock)prepareChangeProfile
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator.
- (void)disconnect;

// Informs the mediator that the Add Account process is done.
- (void)accountAddedIsDone;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_ACCOUNT_MENU_ACCOUNT_MENU_MEDIATOR_H_
