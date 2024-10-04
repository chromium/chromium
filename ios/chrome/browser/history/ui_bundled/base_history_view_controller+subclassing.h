// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_BASE_HISTORY_VIEW_CONTROLLER_SUBCLASSING_H_
#define IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_BASE_HISTORY_VIEW_CONTROLLER_SUBCLASSING_H_

#import "ios/chrome/browser/drag_and_drop/model/table_view_url_drag_drop_handler.h"
#import "ios/chrome/browser/history/ui_bundled/base_history_view_controller.h"

@interface BaseHistoryViewController (Subclassing) <TableViewURLDragDataSource>

// YES if there are no results to show.
@property(nonatomic, assign) BOOL empty;
// YES if the history panel should show a notice about additional forms of
// browsing history.
@property(nonatomic, assign)
    BOOL shouldShowNoticeAboutOtherFormsOfBrowsingHistory;
// YES if there is an outstanding history query.
@property(nonatomic, assign, getter=isLoading) BOOL loading;
// NSMutableArray that holds all indexPaths for entries that will be filtered
// out by the search controller.
@property(nonatomic, strong)
    NSMutableArray<NSIndexPath*>* filteredOutEntriesIndexPaths;
// YES if the table should be filtered by the next received query result.
@property(nonatomic, assign) BOOL filterQueryResult;
// Object to manage insertion of history entries into the table view model.
@property(nonatomic, strong) HistoryEntryInserter* entryInserter;
// Fetches history for search text `query`. If `query` is nil or the empty
// string, all history is fetched. If continuation is false, then the most
// recent results are fetched, otherwise the results more recent than the
// previous query will be returned.
- (void)fetchHistoryForQuery:(NSString*)query continuation:(BOOL)continuation;
// Updates header section to provide relevant information about the currently
// displayed history entries. There should only ever be at most one item in this
// section.
- (void)updateEntriesStatusMessageWithMessage:(NSString*)message
                       messageWillContainLink:(BOOL)messageWillContainLink;
// Clears the background of the TableView.
- (void)removeEmptyTableViewBackground;
// Updates various elements after history items have been deleted from the
// TableView.
- (void)updateTableViewAfterDeletingEntries;
// Search history for text `query` and display the results. `query` may be nil.
// If query is empty, show all history items.
- (void)showHistoryMatchingQuery:(NSString*)query;
// Deletes selected items from browser history and removes them from the
// tableView.
- (void)deleteSelectedItemsFromHistory;
// Adds a view as background of the TableView.
- (void)addEmptyTableViewBackground;
// Filters out entries that should not be displayed.
- (void)filterResults:(NSMutableArray*)resultItems
          searchQuery:(NSString*)searchQuery;
// Checks if there are no results and no URLs have been loaded.
- (BOOL)checkEmptyHistory:
    (const std::vector<BrowsingHistoryService::HistoryEntry>&)results;
// Checks if the loading indicator should be displayed.
- (BOOL)shouldDisplayLoadingIndicator;
// Displays a context menu on the cell pressed with gestureRecognizer.
- (void)displayContextMenuInvokedByGestureRecognizer:
    (UILongPressGestureRecognizer*)gestureRecognizer;

@end

#endif  // IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_BASE_HISTORY_VIEW_CONTROLLER_SUBCLASSING_H_
