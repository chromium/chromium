// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_SYNC_SETTINGS_TABLE_VIEW_CONTROLLER_MODEL_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_SYNC_SETTINGS_TABLE_VIEW_CONTROLLER_MODEL_DELEGATE_H_

@protocol ManageSyncSettingsConsumer;

// Delegate for ManageSyncSettingsTableViewController instance, to manage the
// model.
@protocol ManageSyncSettingsTableViewControllerModelDelegate <NSObject>

// Called when the model should be loaded.
- (void)manageSyncSettingsTableViewControllerLoadModel:
    (id<ManageSyncSettingsConsumer>)controller;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_SYNC_SETTINGS_TABLE_VIEW_CONTROLLER_MODEL_DELEGATE_H_
