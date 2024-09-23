// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_SYNC_SETTINGS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_SYNC_SETTINGS_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/sync/model/sync_observer_bridge.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_service_delegate.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_table_view_controller_model_delegate.h"

class AuthenticationService;
class ChromeAccountManagerService;
class PrefService;
@protocol ManageSyncSettingsCommandHandler;
@protocol ManageSyncSettingsConsumer;
@protocol SyncErrorSettingsCommandHandler;
namespace signin {
class IdentityManager;
}  // namespace signin
namespace syncer {
class SyncService;
}  // namespace syncer

// Mediator for the manager sync settings.
@interface ManageSyncSettingsMediator
    : NSObject <ManageSyncSettingsServiceDelegate,
                ManageSyncSettingsTableViewControllerModelDelegate,
                SyncObserverModelBridge>

// Consumer.
@property(nonatomic, weak) id<ManageSyncSettingsConsumer> consumer;
// Command handler.
@property(nonatomic, weak) id<ManageSyncSettingsCommandHandler> commandHandler;
// The initial account sync state at the time this mediator gets created.
// While the mediator is running it gets updated only if the user signs
// out.
@property(nonatomic, assign, readonly)
    SyncSettingsAccountState initialAccountState;
// Error command handler.
@property(nonatomic, weak) id<SyncErrorSettingsCommandHandler> syncErrorHandler;
// Returns YES if the encryption item should be enabled.
@property(nonatomic, assign, readonly) BOOL shouldEncryptionItemBeEnabled;
// YES if the forced sign-in policy is enabled which requires contextual
// information.
@property(nonatomic, assign) BOOL forcedSigninEnabled;
// YES if the account belongs to an EEA user. Defaults to NO.
@property(nonatomic, assign) BOOL isEEAAccount;
// Returns the default title for the Sync Settings based on the account state.
@property(nonatomic, strong, readonly) NSString* overrideViewControllerTitle;
// Number of local items to upload excluding passwords.
@property(nonatomic, assign) NSInteger localItemsToUpload;
// Number of local passwords to upload.
@property(nonatomic, assign) NSInteger localPasswordsToUpload;

// Designated initializer.
// `syncService`: Sync service. Should not be null.
- (instancetype)
      initWithSyncService:(syncer::SyncService*)syncService
          identityManager:(signin::IdentityManager*)identityManager
    authenticationService:(AuthenticationService*)authenticationService
    accountManagerService:(ChromeAccountManagerService*)accountManagerService
              prefService:(PrefService*)prefService
      initialAccountState:(SyncSettingsAccountState)initialAccountState
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator to all observers and services.
- (void)disconnect;

// Enable or disable Autofill data type.
- (void)autofillAlertConfirmed:(BOOL)value;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_SYNC_SETTINGS_MEDIATOR_H_
