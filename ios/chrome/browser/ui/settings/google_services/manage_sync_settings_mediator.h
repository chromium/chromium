// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_SYNC_SETTINGS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_SYNC_SETTINGS_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/sync/sync_observer_bridge.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_service_delegate.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_view_controller_model_delegate.h"

@protocol SyncErrorSettingsCommandHandler;
@protocol ManageSyncSettingsCommandHandler;
@protocol ManageSyncSettingsConsumer;
class PrefService;
class SyncSetupService;
namespace syncer {
class SyncService;
}  // syncer

// Mediator for the manager sync settings.
@interface ManageSyncSettingsMediator
    : NSObject <ManageSyncSettingsServiceDelegate,
                ManageSyncSettingsTableViewControllerModelDelegate,
                SyncObserverModelBridge>

// Consumer.
@property(nonatomic, weak) id<ManageSyncSettingsConsumer> consumer;
// Sync setup service.
@property(nonatomic, assign) SyncSetupService* syncSetupService;
// Command handler.
@property(nonatomic, weak) id<ManageSyncSettingsCommandHandler> commandHandler;
// Error command handler.
@property(nonatomic, weak) id<SyncErrorSettingsCommandHandler> syncErrorHandler;
// Returns YES if the encryption item should be enabled.
@property(nonatomic, assign, readonly) BOOL shouldEncryptionItemBeEnabled;
// YES if the forced sign-in policy is enabled which requires contextual
// information.
@property(nonatomic, assign) BOOL forcedSigninEnabled;

// Designated initializer.
// `syncService`: Sync service. Should not be null.
- (instancetype)initWithSyncService:(syncer::SyncService*)syncService
                    userPrefService:(PrefService*)userPrefService
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_SYNC_SETTINGS_MEDIATOR_H_
