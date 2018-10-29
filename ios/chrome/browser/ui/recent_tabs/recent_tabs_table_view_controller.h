// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_RECENT_TABS_RECENT_TABS_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_RECENT_TABS_RECENT_TABS_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_consumer.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"

namespace ios {
class ChromeBrowserState;
}

@protocol ApplicationCommands;
@protocol RecentTabsTableViewControllerDelegate;
@protocol RecentTabsPresentationDelegate;
@protocol UrlLoader;
@protocol RecentTabsImageDataSource;

@interface RecentTabsTableViewController
    : ChromeTableViewController<RecentTabsConsumer>
// The coordinator's BrowserState.
@property(nonatomic, assign) ios::ChromeBrowserState* browserState;
// The dispatcher used by this ViewController.
@property(nonatomic, weak) id<ApplicationCommands> dispatcher;
// UrlLoader used by this ViewController.
@property(nonatomic, weak) id<UrlLoader> loader;

// RecentTabsTableViewControllerDelegate delegate.
@property(nonatomic, weak) id<RecentTabsTableViewControllerDelegate> delegate;

// Delegate to present the tab UI.
@property(nonatomic, weak) id<RecentTabsPresentationDelegate>
    presentationDelegate;

// Data source for images.
@property(nonatomic, weak) id<RecentTabsImageDataSource> imageDataSource;

// Initializers.
- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithTableViewStyle:(UITableViewStyle)style
                           appBarStyle:
                               (ChromeTableViewControllerStyle)appBarStyle
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_RECENT_TABS_RECENT_TABS_TABLE_VIEW_CONTROLLER_H_
