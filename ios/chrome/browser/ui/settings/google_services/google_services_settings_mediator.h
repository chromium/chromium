// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_GOOGLE_SERVICES_SETTINGS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_GOOGLE_SERVICES_SETTINGS_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_consumer.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_mode.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_service_delegate.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_view_controller.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_view_controller_model_delegate.h"

class AuthenticationService;
@protocol GoogleServicesSettingsCommandHandler;
@class GoogleServicesSettingsViewController;
class PrefService;
class SyncSetupService;

// Accessibility identifier for Manage Sync cell.
extern NSString* const kManageSyncCellAccessibilityIdentifier;

namespace syncer {
class SyncService;
}  // namespace syncer

namespace signin {
class IdentityManager;
}  // namespace signin

// Mediator for the Google services settings.
@interface GoogleServicesSettingsMediator
    : NSObject <GoogleServicesSettingsServiceDelegate,
                GoogleServicesSettingsViewControllerModelDelegate>

// Google services settings mode.
@property(nonatomic, assign, readonly) GoogleServicesSettingsMode mode;
// View controller.
@property(nonatomic, weak) id<GoogleServicesSettingsConsumer> consumer;
// Authentication service.
@property(nonatomic, assign) AuthenticationService* authService;
// Command handler.
@property(nonatomic, weak) id<GoogleServicesSettingsCommandHandler>
    commandHandler;
// Sync service.
@property(nonatomic, assign) syncer::SyncService* syncService;
// Identity manager;
@property(nonatomic, assign) signin::IdentityManager* identityManager;

// Designated initializer. All the paramters should not be null.
// |userPrefService|: preference service from the browser state.
// |localPrefService|: preference service from the application context.
// |syncSetupService|: allows configuring sync.
// |mode|: mode to display the Google services settings.
- (instancetype)initWithUserPrefService:(PrefService*)userPrefService
                       localPrefService:(PrefService*)localPrefService
                       syncSetupService:(SyncSetupService*)syncSetupService
                                   mode:(GoogleServicesSettingsMode)mode
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_GOOGLE_SERVICES_SETTINGS_MEDIATOR_H_
