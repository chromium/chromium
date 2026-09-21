// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_SITE_SETTINGS_UI_SITE_SETTINGS_CATEGORY_DETAIL_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SETTINGS_SITE_SETTINGS_UI_SITE_SETTINGS_CATEGORY_DETAIL_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/settings/site_settings/public/site_settings_constants.h"
#import "ios/chrome/browser/settings/site_settings/ui/site_settings_category_detail_consumer.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_root_table_view_controller.h"

@class SiteSettingsCategoryDetailViewController;
@protocol SiteSettingsCategoryDetailMutator;
@protocol TableViewFaviconDataSource;

// Delegate for SiteSettingsCategoryDetailViewController.
@protocol SiteSettingsCategoryDetailViewControllerDelegate <NSObject>

// Notifies the delegate that the view controller was removed from its parent.
- (void)siteSettingsCategoryDetailViewControllerWasRemoved:
    (SiteSettingsCategoryDetailViewController*)controller;

@end

// View controller displaying details for a specific site settings category.
@interface SiteSettingsCategoryDetailViewController
    : SettingsRootTableViewController <SiteSettingsCategoryDetailConsumer>

// Delegate for this view controller.
@property(nonatomic, weak) id<SiteSettingsCategoryDetailViewControllerDelegate>
    delegate;

// Mutator for user actions.
@property(nonatomic, weak) id<SiteSettingsCategoryDetailMutator> mutator;

// Data source for loading site favicons.
@property(nonatomic, weak) id<TableViewFaviconDataSource> imageDataSource;

// Initializer specifying the category.
- (instancetype)initWithCategory:(SiteSettingsCategory)category
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_SITE_SETTINGS_UI_SITE_SETTINGS_CATEGORY_DETAIL_VIEW_CONTROLLER_H_
