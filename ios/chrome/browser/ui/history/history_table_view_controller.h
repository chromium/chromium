// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"

#include "ios/chrome/browser/ui/history/history_consumer.h"
#import "ios/chrome/browser/ui/history/history_entry_item_delegate.h"

class Browser;
enum class UrlLoadStrategy;

@class ActionSheetCoordinator;
@protocol TableViewFaviconDataSource;
@protocol HistoryMenuProvider;
@protocol HistoryUIDelegate;
@protocol HistoryPresentationDelegate;

// ChromeTableViewController for displaying history items.
@interface HistoryTableViewController
    : ChromeTableViewController <HistoryConsumer,
                                 HistoryEntryItemDelegate,
                                 UIAdaptivePresentationControllerDelegate>
// The ViewController's Browser.
@property(nonatomic, assign) Browser* browser;
// Abstraction to communicate with HistoryService and WebHistoryService.
// Not owned by HistoryTableViewController.
@property(nonatomic, assign) history::BrowsingHistoryService* historyService;
// Opaque instructions on how to open urls.
@property(nonatomic) UrlLoadStrategy loadStrategy;
// Optional: If provided, search terms to filter the displayed history items.
// `searchTerms` will be the initial value of the text in the search bar.
@property(nonatomic, copy) NSString* searchTerms;
// Delegate for this HistoryTableView.
@property(nonatomic, weak) id<HistoryUIDelegate> delegate;
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

@end

#endif  // IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_TABLE_VIEW_CONTROLLER_H_
