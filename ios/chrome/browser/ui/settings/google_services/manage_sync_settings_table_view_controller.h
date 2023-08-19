// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_SYNC_SETTINGS_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_SYNC_SETTINGS_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_consumer.h"

@class ManageSyncSettingsTableViewController;
@protocol ManageSyncSettingsTableViewControllerModelDelegate;
@protocol ManageSyncSettingsServiceDelegate;

// Delegate for presentation events related to
// ManageSyncSettingsTableViewController.
@protocol ManageSyncSettingsTableViewControllerPresentationDelegate <NSObject>

// Called when the view controller is removed from its parent.
- (void)manageSyncSettingsTableViewControllerWasRemoved:
    (ManageSyncSettingsTableViewController*)controller;

@end

// View controller to related to Manage sync settings view.
@interface ManageSyncSettingsTableViewController
    : SettingsRootTableViewController <ManageSyncSettingsConsumer>

// Presentation delegate.
@property(nonatomic, weak)
    id<ManageSyncSettingsTableViewControllerPresentationDelegate>
        presentationDelegate;
// Model delegate.
@property(nonatomic, weak)
    id<ManageSyncSettingsTableViewControllerModelDelegate>
        modelDelegate;
// Service delegate.
@property(nonatomic, weak) id<ManageSyncSettingsServiceDelegate>
    serviceDelegate;

// Tells the controller if it is being viewed for a signed-in not syncing
// account or not.
@property(nonatomic, assign) BOOL isAccountStateSignedIn;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_SYNC_SETTINGS_TABLE_VIEW_CONTROLLER_H_
