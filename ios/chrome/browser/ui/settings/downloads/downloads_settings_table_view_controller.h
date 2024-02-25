// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_DOWNLOADS_DOWNLOADS_SETTINGS_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_DOWNLOADS_DOWNLOADS_SETTINGS_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/downloads/save_to_photos/save_to_photos_settings_account_confirmation_consumer.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

@protocol DownloadsSettingsTableViewControllerActionDelegate;
@protocol DownloadsSettingsTableViewControllerPresentationDelegate;
@protocol SaveToPhotosSettingsMutator;

// Table view controller for Downloads settings.
@interface DownloadsSettingsTableViewController
    : SettingsRootTableViewController <
          SaveToPhotosSettingsAccountConfirmationConsumer>

// Mutator.
@property(nonatomic, weak) id<SaveToPhotosSettingsMutator>
    saveToPhotosSettingsMutator;

// Delegates.
@property(nonatomic, weak)
    id<DownloadsSettingsTableViewControllerActionDelegate>
        actionDelegate;
@property(nonatomic, weak)
    id<DownloadsSettingsTableViewControllerPresentationDelegate>
        presentationDelegate;

// Initialization.
- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_DOWNLOADS_DOWNLOADS_SETTINGS_TABLE_VIEW_CONTROLLER_H_
