// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_SYNC_SETTINGS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_SYNC_SETTINGS_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"
#import "ios/chrome/browser/ui/settings/google_services/sync_settings_view_state.h"

@class ManageSyncSettingsCoordinator;
@protocol SyncSettingsViewState;

// Delegate for ManageSyncSettingsCoordinator.
@protocol ManageSyncSettingsCoordinatorDelegate <NSObject>

// Called when the view controller is removed from its parent.
- (void)manageSyncSettingsCoordinatorWasRemoved:
    (ManageSyncSettingsCoordinator*)coordinator;

// Title for the Sync Settings coordinator.
// TODO(crbug.com/1222632): Remove property following the MICE Settings launch.
@property(nonatomic, readonly) NSString* manageSyncSettingsCoordinatorTitle;

@end

// Coordinator for the Manage Sync Settings TableView Controller.
// This class doesn't commit any sync changes made by the user. This class
// relies on GoogleServicesSettingsCoordinator to commit the sync changes.
@interface ManageSyncSettingsCoordinator
    : ChromeCoordinator <SyncSettingsViewState>

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;

// Delegate.
@property(nonatomic, weak) id<ManageSyncSettingsCoordinatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_SYNC_SETTINGS_COORDINATOR_H_
