// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_SYNC_SETTINGS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_SYNC_SETTINGS_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"

@class ManageSyncSettingsCoordinator;

// Delegate for ManageSyncSettingsCoordinator.
@protocol ManageSyncSettingsCoordinatorDelegate <NSObject>

// Called when the view controller is removed from its parent.
- (void)manageSyncSettingsCoordinatorWasRemoved:
    (ManageSyncSettingsCoordinator*)coordinator;

// Title for the Sync Settings coordinator.
// TODO(crbug.com/40774188): Remove property following the MICE Settings launch.
@property(nonatomic, readonly) NSString* manageSyncSettingsCoordinatorTitle;

@optional
// Called when the view controller is about to open the Chrome Sync web page
// URL.
- (void)manageSyncSettingsCoordinatorNeedToOpenChromeSyncWebPage:
    (ManageSyncSettingsCoordinator*)coordinator;

@end

// Coordinator for the Manage Sync Settings TableView Controller.
// This class doesn't commit any sync changes made by the user. This class
// relies on GoogleServicesSettingsCoordinator to commit the sync changes.
@interface ManageSyncSettingsCoordinator : ChromeCoordinator

// Delegate.
@property(nonatomic, weak) id<ManageSyncSettingsCoordinatorDelegate> delegate;

// Initializes ManageSyncSettingsCoordinator to view its controller by pushing
// it on top of the navigation stack.
- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                    accountState:
                                        (SyncSettingsAccountState)accountState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_SYNC_SETTINGS_COORDINATOR_H_
