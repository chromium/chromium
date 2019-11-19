// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"

#include "ios/chrome/browser/ui/history/history_consumer.h"

namespace ios {
class ChromeBrowserState;
}

enum class UrlLoadStrategy;

@class ContextMenuCoordinator;
@protocol TableViewFaviconDataSource;
@protocol HistoryLocalCommands;
@protocol HistoryPresentationDelegate;

// ChromeTableViewController for displaying history items.
@interface HistoryTableViewController
    : ChromeTableViewController <HistoryConsumer,
                                 UIAdaptivePresentationControllerDelegate>
// The ViewController's BrowserState.
@property(nonatomic, assign) ios::ChromeBrowserState* browserState;
// Abstraction to communicate with HistoryService and WebHistoryService.
// Not owned by HistoryTableViewController.
@property(nonatomic, assign) history::BrowsingHistoryService* historyService;
// Opaque instructions on how to open urls.
@property(nonatomic) UrlLoadStrategy loadStrategy;
// Delegate for this HistoryTableView.
@property(nonatomic, weak) id<HistoryLocalCommands> localDispatcher;
// Delegate used to make the Tab UI visible.
@property(nonatomic, weak) id<HistoryPresentationDelegate> presentationDelegate;
// Data source for favicon images.
@property(nonatomic, weak) id<TableViewFaviconDataSource> imageDataSource;
// Coordinator for displaying context menus for history entries.
@property(nonatomic, strong) ContextMenuCoordinator* contextMenuCoordinator;

// Initializers.
- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithTableViewStyle:(UITableViewStyle)style
                           appBarStyle:
                               (ChromeTableViewControllerStyle)appBarStyle
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_TABLE_VIEW_CONTROLLER_H_
