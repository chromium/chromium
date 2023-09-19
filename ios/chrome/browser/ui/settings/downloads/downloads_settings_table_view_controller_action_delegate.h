// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_DOWNLOADS_DOWNLOADS_SETTINGS_TABLE_VIEW_CONTROLLER_ACTION_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_DOWNLOADS_DOWNLOADS_SETTINGS_TABLE_VIEW_CONTROLLER_ACTION_DELEGATE_H_

@class DownloadsSettingsTableViewController;

// Action delegate for DownloadsSettingsTableViewController.
@protocol DownloadsSettingsTableViewControllerActionDelegate <NSObject>

// Invoked when the user tapped the button to open the account selection view.
- (void)downloadsSettingsTableViewControllerOpenSaveToPhotosAccountSelection:
    (DownloadsSettingsTableViewController*)controller;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_DOWNLOADS_DOWNLOADS_SETTINGS_TABLE_VIEW_CONTROLLER_ACTION_DELEGATE_H_
