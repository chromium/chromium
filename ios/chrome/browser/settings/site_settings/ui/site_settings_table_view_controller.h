// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_SITE_SETTINGS_UI_SITE_SETTINGS_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SETTINGS_SITE_SETTINGS_UI_SITE_SETTINGS_TABLE_VIEW_CONTROLLER_H_

#import "components/content_settings/core/common/content_settings_types.h"
#import "ios/chrome/browser/settings/site_settings/ui/site_settings_consumer.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_root_table_view_controller.h"

@class SiteSettingsTableViewController;

// Delegate protocol for SiteSettingsTableViewController.
@protocol SiteSettingsTableViewControllerDelegate <NSObject>

// Notifies the delegate that the view controller was removed from its parent.
- (void)siteSettingsTableViewControllerWasRemoved:
    (SiteSettingsTableViewController*)controller;

// Notifies the delegate that a setting type was selected.
- (void)siteSettingsTableViewController:
            (SiteSettingsTableViewController*)controller
                   didSelectSettingType:(ContentSettingsType)type;

@end

// View controller for the root Site Settings screen displaying permission
// categories.
@interface SiteSettingsTableViewController
    : SettingsRootTableViewController <SiteSettingsConsumer>

// Delegate for this view controller.
@property(nonatomic, weak) id<SiteSettingsTableViewControllerDelegate> delegate;

- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_SITE_SETTINGS_UI_SITE_SETTINGS_TABLE_VIEW_CONTROLLER_H_
