// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_SITE_PERMISSIONS_SITE_PERMISSIONS_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_SITE_PERMISSIONS_SITE_PERMISSIONS_TABLE_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/settings/ui_bundled/settings_root_table_view_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/site_permissions/site_permissions_consumer.h"

@class SitePermissionsSiteItem;
@class SitePermissionsTableViewController;
@protocol TableViewFaviconDataSource;

// Delegate for SitePermissionsTableViewController actions.
@protocol SitePermissionsTableViewControllerDelegate <NSObject>

// Notifies the delegate that the view controller was removed from its parent.
- (void)sitePermissionsTableViewControllerWasRemoved:
    (SitePermissionsTableViewController*)controller;

// Notifies the delegate that a site entry was selected.
- (void)sitePermissionsTableViewController:
            (SitePermissionsTableViewController*)controller
                             didSelectSite:(SitePermissionsSiteItem*)siteItem;

@end

// View controller displaying a searchable list of sites with configured
// permissions.
@interface SitePermissionsTableViewController
    : SettingsRootTableViewController <SitePermissionsConsumer,
                                       UISearchResultsUpdating>

// Designated initializer.
- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

// Delegate for user actions and lifecycle events.
@property(nonatomic, weak) id<SitePermissionsTableViewControllerDelegate>
    delegate;

// Data source for site favicons.
@property(nonatomic, weak) id<TableViewFaviconDataSource> imageDataSource;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_SITE_PERMISSIONS_SITE_PERMISSIONS_TABLE_VIEW_CONTROLLER_H_
