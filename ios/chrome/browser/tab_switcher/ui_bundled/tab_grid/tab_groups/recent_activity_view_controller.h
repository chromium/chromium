// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_RECENT_ACTIVITY_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_RECENT_ACTIVITY_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/recent_activity_consumer.h"

@protocol ApplicationCommands;
@protocol RecentActivityMutator;
@protocol TableViewFaviconDataSource;

// A view controller that contains recent activity logs in a shared tab group.
@interface RecentActivityViewController
    : ChromeTableViewController <RecentActivityConsumer>

// Handler for application commands.
@property(nonatomic, weak) id<ApplicationCommands> applicationHandler;

@property(nonatomic, weak) id<RecentActivityMutator> mutator;

// Data source for favicons.
@property(nonatomic, weak) id<TableViewFaviconDataSource> faviconDataSource;

- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_RECENT_ACTIVITY_VIEW_CONTROLLER_H_
