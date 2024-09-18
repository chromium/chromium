// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_BASE_HISTORY_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_BASE_HISTORY_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/drag_and_drop/model/table_view_url_drag_drop_handler.h"
#import "ios/chrome/browser/history/ui_bundled/history_consumer.h"
#import "ios/chrome/browser/history/ui_bundled/history_entry_inserter.h"
#import "ios/chrome/browser/history/ui_bundled/history_entry_item_delegate.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"

class Browser;
enum class UrlLoadStrategy;

@class ActionSheetCoordinator;
@protocol TableViewFaviconDataSource;
@protocol HistoryMenuProvider;
@protocol HistoryTableViewControllerDelegate;
@protocol HistoryPresentationDelegate;

using history::BrowsingHistoryService;

// LegacyChromeTableViewController for displaying history items.
@interface BaseHistoryViewController
    : LegacyChromeTableViewController <HistoryConsumer,
                                       HistoryEntryItemDelegate,
                                       UIAdaptivePresentationControllerDelegate>

// The ViewController's Browser.
@property(nonatomic, assign) Browser* browser;
// Abstraction to communicate with HistoryService and WebHistoryService.
// Not owned by HistoryTableViewController.
@property(nonatomic, assign) history::BrowsingHistoryService* historyService;
// Opaque instructions on how to open urls.
@property(nonatomic) UrlLoadStrategy loadStrategy;
// Delegate for this HistoryTableView.
@property(nonatomic, weak) id<HistoryTableViewControllerDelegate> delegate;
// Delegate used to make the Tab UI visible.
@property(nonatomic, weak) id<HistoryPresentationDelegate> presentationDelegate;
// Data source for favicon images.
@property(nonatomic, weak) id<TableViewFaviconDataSource> imageDataSource;
// Coordinator for displaying a context menu for history entries.
@property(nonatomic, strong) ActionSheetCoordinator* contextMenuCoordinator;
// Provider of menu configurations for the history component.
@property(nonatomic, weak) id<HistoryMenuProvider> menuProvider API_AVAILABLE(
    ios(13.0));

// Initializers.
- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

// Call this when the view controller needs to be detached from browser
// synchronously (in the case of a shutdown for example).
- (void)detachFromBrowser;
@end

#endif  // IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_BASE_HISTORY_VIEW_CONTROLLER_H_
