// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_RECENT_TABS_UI_RECENT_TABS_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_RECENT_TABS_UI_RECENT_TABS_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/recent_tabs/ui/recent_tabs_consumer.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"
#include "ui/base/window_open_disposition.h"

class Browser;
enum class UrlLoadStrategy;

namespace synced_sessions {
struct DistantSession;
}

@protocol ApplicationCommands;
@protocol SettingsCommands;
@protocol RecentTabsMenuProvider;
@protocol RecentTabsPresentationDelegate;
@protocol TableViewFaviconDataSource;

@interface RecentTabsTableViewController
    : LegacyChromeTableViewController <RecentTabsConsumer,
                                       UIAdaptivePresentationControllerDelegate>
// The Browser for the tabs being restored. It's an error to pass a nullptr
// Browser.
@property(nonatomic, assign) Browser* browser;
// The command handlers used by this ViewController.
@property(nonatomic, weak) id<ApplicationCommands> applicationHandler;
@property(nonatomic, weak) id<SettingsCommands> settingsHandler;

// Opaque instructions on how to open urls.
@property(nonatomic) UrlLoadStrategy loadStrategy;
// Whether the updates of the RecentTabs should be ignored. Setting this to NO
// would trigger a reload of the TableView.
@property(nonatomic, assign) BOOL preventUpdates;

// Delegate to present the tab UI.
@property(nonatomic, weak) id<RecentTabsPresentationDelegate>
    presentationDelegate;

// Data source for images. Must be set before the cells are configured for the
// first time.
@property(nonatomic, weak) id<TableViewFaviconDataSource> imageDataSource;

// Provider of menu configurations for the recentTabs component.
@property(nonatomic, weak) id<RecentTabsMenuProvider> menuProvider;

// Multi-window session for this vc's recent tabs.
@property(nonatomic, weak) UISceneSession* session;

// Initializers.
- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

// Returns Sessions corresponding to the given `sectionIdentifier`.
- (synced_sessions::DistantSession const*)sessionForTableSectionWithIdentifier:
    (NSInteger)sectionIdentifer;

// Hides Sessions corresponding to the given the table view's
// `sectionIdentifier`.
- (void)removeSessionAtTableSectionWithIdentifier:(NSInteger)sectionIdentifier;

@end

#endif  // IOS_CHROME_BROWSER_RECENT_TABS_UI_RECENT_TABS_TABLE_VIEW_CONTROLLER_H_
