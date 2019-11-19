// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_SYNC_SETTINGS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_SYNC_SETTINGS_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

@protocol ApplicationCommands;
@protocol BrowserCommands;
@class ManageSyncSettingsCoordinator;

// Delegate for ManageSyncSettingsCoordinator.
@protocol ManageSyncSettingsCoordinatorDelegate <NSObject>

// Called when the view controller is popped out from navigation controller.
- (void)manageSyncSettingsCoordinatorWasPopped:
    (ManageSyncSettingsCoordinator*)coordinator;

@end

// Coordinator for the Manage Sync Settings TableView Controller.
// This class doesn't commit any sync changes made by the user. This class
// relies on GoogleServicesSettingsCoordinator to commit the sync changes.
@interface ManageSyncSettingsCoordinator : ChromeCoordinator

// Delegate.
@property(nonatomic, weak) id<ManageSyncSettingsCoordinatorDelegate> delegate;
// Global dispatcher.
@property(nonatomic, weak) id<ApplicationCommands, BrowserCommands> dispatcher;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_SYNC_SETTINGS_COORDINATOR_H_
