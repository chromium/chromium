// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_DOWNLOADS_DOWNLOADS_SETTINGS_TABLE_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_DOWNLOADS_DOWNLOADS_SETTINGS_TABLE_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_

#import <Foundation/Foundation.h>

@class DownloadsSettingsTableViewController;

// Presentation delegate for DownloadsSettingsTableViewController.
@protocol DownloadsSettingsTableViewControllerPresentationDelegate <NSObject>

// Invoked when the DownloadsSettingsTableViewController was dismissed.
- (void)downloadsSettingsTableViewControllerWasRemoved:
    (DownloadsSettingsTableViewController*)controller;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_DOWNLOADS_DOWNLOADS_SETTINGS_TABLE_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_
