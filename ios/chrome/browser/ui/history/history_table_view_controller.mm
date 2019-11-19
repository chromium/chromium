// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/history/history_table_view_controller.h"

#include "base/i18n/time_formatting.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_formatter.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/metrics/new_tab_page_uma.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/context_menu/context_menu_coordinator.h"
#include "ios/chrome/browser/ui/history/history_entries_status_item.h"
#import "ios/chrome/browser/ui/history/history_entries_status_item_delegate.h"
#include "ios/chrome/browser/ui/history/history_entry_inserter.h"
#import "ios/chrome/browser/ui/history/history_entry_item.h"
#import "ios/chrome/browser/ui/history/history_entry_item_delegate.h"
#include "ios/chrome/browser/ui/history/history_local_commands.h"
#import "ios/chrome/browser/ui/history/history_ui_constants.h"
#include "ios/chrome/browser/ui/history/history_util.h"
#import "ios/chrome/browser/ui/history/public/history_presentation_delegate.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_link_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/ui/table_view/table_view_favicon_data_source.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller_constants.h"
#import "ios/chrome/browser/ui/util/pasteboard_util.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/browser/url_loading/url_loading_service.h"
#import "ios/chrome/browser/url_loading/url_loading_service_factory.h"
#import "ios/chrome/common/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#import "ios/chrome/common/favicon/favicon_view.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/referrer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using history::BrowsingHistoryService;

namespace {
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeHistoryEntry = kItemTypeEnumZero,
  ItemTypeEntriesStatus,
  ItemTypeEntriesStatusWithLink,
  ItemTypeActivityIndicator,
};
// Section identifier for the header (sync information) section.
const NSInteger kEntriesStatusSectionIdentifier = kSectionIdentifierEnumZero;
// Maximum number of entries to retrieve in a single query to history service.
const int kMaxFetchCount = 100;
// Separation space between sections.
const CGFloat kSeparationSpaceBetweenSections = 9;
// The default UIButton font size used by UIKit.
const CGFloat kButtonDefaultFontSize = 15.0;
// Horizontal width representing UIButton's padding.
const CGFloat kButtonHorizontalPadding = 30.0;
}  // namespace

@interface HistoryTableViewController ()<HistoryEntriesStatusItemDelegate,
                                         HistoryEntryInserterDelegate,
                                         HistoryEntryItemDelegate,
                                         TableViewTextLinkCellDelegate,
                                         UISearchControllerDelegate,
                                         UISearchResultsUpdating,
                                         UISearchBarDelegate> {
  // Closure to request next page of history.
  base::OnceClosure _query_history_continuation;
}

// Object to manage insertion of history entries into the table view model.
@property(nonatomic, strong) HistoryEntryInserter* entryInserter;
// The current query for visible history entries.
@property(nonatomic, copy) NSString* currentQuery;
// The current status message for the tableView, it might be nil.
@property(nonatomic, copy) NSString* currentStatusMessage;
// YES if there are no results to show.
@property(nonatomic, assign) BOOL empty;
// YES if the history panel should show a notice about additional forms of
// browsing history.
@property(nonatomic, assign)
    BOOL shouldShowNoticeAboutOtherFormsOfBrowsingHistory;
// YES if there is an outstanding history query.
@property(nonatomic, assign, getter=isLoading) BOOL loading;
// YES if there is a search happening.
@property(nonatomic, assign) BOOL searchInProgress;
// NSMutableArray that holds all indexPaths for entries that will be filtered
// out by the search controller.
@property(nonatomic, strong)
    NSMutableArray<NSIndexPath*>* filteredOutEntriesIndexPaths;
// YES if there are no more history entries to load.
@property(nonatomic, assign, getter=hasFinishedLoading) BOOL finishedLoading;
// YES if the table should be filtered by the next received query result.
@property(nonatomic, assign) BOOL filterQueryResult;
// This ViewController's searchController;
@property(nonatomic, strong) UISearchController* searchController;
// NavigationController UIToolbar Buttons.
@property(nonatomic, strong) UIBarButtonItem* cancelButton;
@property(nonatomic, strong) UIBarButtonItem* clearBrowsingDataButton;
@property(nonatomic, strong) UIBarButtonItem* deleteButton;
@property(nonatomic, strong) UIBarButtonItem* editButton;
// Scrim when search box in focused.
@property(nonatomic, strong) UIControl* scrimView;
@end

@implementation HistoryTableViewController

#pragma mark - ViewController Lifecycle.

- (instancetype)init {
  return [super initWithTableViewStyle:UITableViewStylePlain
                           appBarStyle:ChromeTableViewControllerStyleNoAppBar];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  // Center search bar's cancel button vertically so it looks centered.
  // We change the cancel button proxy styles, so we will return it to
  // default in viewDidDisappear.
  UIOffset offset =
      UIOffsetMake(0.0f, kTableViewNavigationVerticalOffsetForSearchHeader);
  UIBarButtonItem* cancelButton = [UIBarButtonItem
      appearanceWhenContainedInInstancesOfClasses:@[ [UISearchBar class] ]];
  [cancelButton setTitlePositionAdjustment:offset
                             forBarMetrics:UIBarMetricsDefault];
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];

  // Restore to default origin offset for cancel button proxy style.
  UIBarButtonItem* cancelButton = [UIBarButtonItem
      appearanceWhenContainedInInstancesOfClasses:@[ [UISearchBar class] ]];
  [cancelButton setTitlePositionAdjustment:UIOffsetZero
                             forBarMetrics:UIBarMetricsDefault];
}

- (void)viewDidLoad {
  [super viewDidLoad];
  [self loadModel];

  // TableView configuration
  self.tableView.estimatedRowHeight = 56;
  self.tableView.rowHeight = UITableViewAutomaticDimension;
  self.tableView.estimatedSectionHeaderHeight = 56;
  self.tableView.sectionFooterHeight = 0.0;
  self.tableView.keyboardDismissMode = UIScrollViewKeyboardDismissModeOnDrag;
  self.tableView.allowsMultipleSelectionDuringEditing = YES;
  self.clearsSelectionOnViewWillAppear = NO;
  self.tableView.allowsMultipleSelection = YES;
  // Add a tableFooterView in order to disable separators at the bottom of the
  // tableView.
  self.tableView.tableFooterView = [[UIView alloc] init];
  self.tableView.accessibilityIdentifier = kHistoryTableViewIdentifier;

  // ContextMenu gesture recognizer.
  UILongPressGestureRecognizer* longPressRecognizer = [
      [UILongPressGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(displayContextMenuInvokedByGestureRecognizer:)];
  [self.tableView addGestureRecognizer:longPressRecognizer];

  // NavigationController configuration.
  self.title = l10n_util::GetNSString(IDS_HISTORY_TITLE);
  // Configures NavigationController Toolbar buttons.
  [self configureViewsForNonEditModeWithAnimation:NO];
  // Adds the "Done" button and hooks it up to |dismissHistory|.
  UIBarButtonItem* dismissButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(dismissHistory)];
  [dismissButton setAccessibilityIdentifier:
                     kHistoryNavigationControllerDoneButtonIdentifier];
  self.navigationItem.rightBarButtonItem = dismissButton;

  // SearchController Configuration.
  // Init the searchController with nil so the results are displayed on the same
  // TableView.
  self.searchController =
      [[UISearchController alloc] initWithSearchResultsController:nil];
  self.searchController.obscuresBackgroundDuringPresentation = NO;
  self.searchController.searchBar.delegate = self;
  self.searchController.searchResultsUpdater = self;
  self.searchController.searchBar.backgroundColor = UIColor.clearColor;
  self.searchController.searchBar.accessibilityIdentifier =
      kHistorySearchControllerSearchBarIdentifier;
  // UIKit needs to know which controller will be presenting the
  // searchController. If we don't add this trying to dismiss while
  // SearchController is active will fail.
  self.definesPresentationContext = YES;

  self.scrimView = [[UIControl alloc] init];
  self.scrimView.alpha = 0.0f;
  self.scrimView.backgroundColor = [UIColor colorNamed:kScrimBackgroundColor];
  self.scrimView.translatesAutoresizingMaskIntoConstraints = NO;
  self.scrimView.accessibilityIdentifier = kHistorySearchScrimIdentifier;
  [self.scrimView addTarget:self
                     action:@selector(dismissSearchController:)
           forControlEvents:UIControlEventTouchUpInside];

  // Place the search bar in the navigation bar.
  self.navigationItem.searchController = self.searchController;
  self.navigationItem.hidesSearchBarWhenScrolling = NO;

  // Center search bar and cancel button vertically so it looks centered
  // in the header when searching.
  UIOffset offset =
      UIOffsetMake(0.0f, kTableViewNavigationVerticalOffsetForSearchHeader);
  self.searchController.searchBar.searchFieldBackgroundPositionAdjustment =
      offset;
}

#pragma mark - TableViewModel

- (void)loadModel {
  [super loadModel];
  // Add Status section, this section will always exist during the lifetime of
  // HistoryTableVC. Its content will be driven by |updateEntriesStatusMessage|.
  [self.tableViewModel
      addSectionWithIdentifier:kEntriesStatusSectionIdentifier];
  _entryInserter =
      [[HistoryEntryInserter alloc] initWithModel:self.tableViewModel];
  _entryInserter.delegate = self;
  _empty = YES;
  [self showHistoryMatchingQuery:nil];
}

#pragma mark - Protocols

#pragma mark HistoryConsumer

- (void)historyQueryWasCompletedWithResults:
            (const std::vector<BrowsingHistoryService::HistoryEntry>&)results
                           queryResultsInfo:
                               (const BrowsingHistoryService::QueryResultsInfo&)
                                   queryResultsInfo
                        continuationClosure:
                            (base::OnceClosure)continuationClosure {
  self.loading = NO;
  _query_history_continuation = std::move(continuationClosure);

  // If history sync is enabled and there hasn't been a response from synced
  // history, try fetching again.
  SyncSetupService* syncSetupService =
      SyncSetupServiceFactory::GetForBrowserState(_browserState);
  if (syncSetupService->IsSyncEnabled() &&
      syncSetupService->IsDataTypeActive(syncer::HISTORY_DELETE_DIRECTIVES) &&
      queryResultsInfo.sync_timed_out) {
    [self showHistoryMatchingQuery:_currentQuery];
    return;
  }

  // At this point there has been a response, stop the loading indicator.
  [self stopLoadingIndicatorWithCompletion:nil];

  // If there are no results and no URLs have been loaded, report that no
  // history entries were found.
  if (results.empty() && self.empty && !self.searchInProgress) {
    UIImage* emptyImage = [[UIImage imageNamed:@"empty_history"]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    [self addEmptyTableViewWithMessage:l10n_util::GetNSString(
                                           IDS_HISTORY_NO_RESULTS)
                                 image:emptyImage];
    [self updateToolbarButtons];
    return;
  }

  self.finishedLoading = queryResultsInfo.reached_beginning;
  self.empty = NO;
  [self removeEmptyTableView];

  // Header section should be updated outside of batch updates, otherwise
  // loading indicator removal will not be observed.
  [self updateEntriesStatusMessage];

  NSMutableArray* resultsItems = [NSMutableArray array];
  NSString* searchQuery =
      [base::SysUTF16ToNSString(queryResultsInfo.search_text) copy];

  // There should always be at least a header section present.
  DCHECK([[self tableViewModel] numberOfSections]);
  for (const BrowsingHistoryService::HistoryEntry& entry : results) {
    HistoryEntryItem* item =
        [[HistoryEntryItem alloc] initWithType:ItemTypeHistoryEntry
                         accessibilityDelegate:self];
    item.text = [history::FormattedTitle(entry.title, entry.url) copy];
    item.detailText =
        [base::SysUTF8ToNSString(entry.url.GetOrigin().spec()) copy];
    item.timeText =
        [base::SysUTF16ToNSString(base::TimeFormatTimeOfDay(entry.time)) copy];
    item.URL = entry.url;
    item.timestamp = entry.time;
    [resultsItems addObject:item];
  }

  [self updateToolbarButtons];

  if ((self.searchInProgress && [searchQuery length] > 0 &&
       [self.currentQuery isEqualToString:searchQuery]) ||
      self.filterQueryResult) {
    // If in search mode, filter out entries that are not part of the
    // search result.
    [self filterForHistoryEntries:resultsItems];
    [self
        deleteItemsFromTableViewModelWithIndex:self.filteredOutEntriesIndexPaths
                      deleteItemsFromTableView:NO];
    // Clear all objects that were just deleted from the tableViewModel.
    [self.filteredOutEntriesIndexPaths removeAllObjects];
    self.filterQueryResult = NO;
  }

  // Insert result items into the model.
  for (HistoryEntryItem* item in resultsItems) {
    [self.entryInserter insertHistoryEntryItem:item];
  }

  // Save the currently selected rows to preserve its state after the tableView
  // is reloaded. Since a query with selected rows can only happen when
  // scrolling down the tableView this should be safe. If this changes in the
  // future e.g. being able to search while selected rows exist, we should
  // update this.
  NSIndexPath* currentSelectedCells = [self.tableView indexPathForSelectedRow];
  [self.tableView reloadData];
  [self.tableView selectRowAtIndexPath:currentSelectedCells
                              animated:NO
                        scrollPosition:UITableViewScrollPositionNone];
  [self updateTableViewAfterDeletingEntries];
}

- (void)showNoticeAboutOtherFormsOfBrowsingHistory:(BOOL)shouldShowNotice {
  self.shouldShowNoticeAboutOtherFormsOfBrowsingHistory = shouldShowNotice;
  // Update the history entries status message if there is no query in progress.
  if (!self.isLoading) {
    [self updateEntriesStatusMessage];
  }
}

- (void)historyWasDeleted {
  // If history has been deleted, reload history filtering for the current
  // results. This only observes local changes to history, i.e. removing
  // history via the clear browsing data page.
  self.filterQueryResult = YES;
  [self showHistoryMatchingQuery:nil];
}

#pragma mark HistoryEntriesStatusItemDelegate

- (void)historyEntriesStatusItem:(HistoryEntriesStatusItem*)item
               didRequestOpenURL:(const GURL&)URL {
  // TODO(crbug.com/805190): Migrate. This will navigate to the status message
  // "Show Full History" URL.
}

#pragma mark HistoryEntryInserterDelegate

- (void)historyEntryInserter:(HistoryEntryInserter*)inserter
    didInsertItemAtIndexPath:(NSIndexPath*)indexPath {
  // NO-OP since [self.tableView reloadData] will be called after the inserter
  // has completed its updates.
}

- (void)historyEntryInserter:(HistoryEntryInserter*)inserter
     didInsertSectionAtIndex:(NSInteger)sectionIndex {
  // NO-OP since [self.tableView reloadData] will be called after the inserter
  // has completed its updates.
}

- (void)historyEntryInserter:(HistoryEntryInserter*)inserter
     didRemoveSectionAtIndex:(NSInteger)sectionIndex {
  // NO-OP since [self.tableView reloadData] will be called after the inserter
  // has completed its updates.
}

#pragma mark HistoryEntryItemDelegate

- (void)historyEntryItemDidRequestOpen:(HistoryEntryItem*)item {
  [self openURL:item.URL];
}

- (void)historyEntryItemDidRequestDelete:(HistoryEntryItem*)item {
  NSInteger sectionIdentifier =
      [self.entryInserter sectionIdentifierForTimestamp:item.timestamp];
  if ([self.tableViewModel hasSectionForSectionIdentifier:sectionIdentifier] &&
      [self.tableViewModel hasItem:item
           inSectionWithIdentifier:sectionIdentifier]) {
    NSIndexPath* indexPath = [self.tableViewModel indexPathForItem:item];
    [self.tableView selectRowAtIndexPath:indexPath
                                animated:NO
                          scrollPosition:UITableViewScrollPositionNone];
    [self deleteSelectedItemsFromHistory];
  }
}

- (void)historyEntryItemDidRequestCopy:(HistoryEntryItem*)item {
  StoreURLInPasteboard(item.URL);
}

- (void)historyEntryItemDidRequestOpenInNewTab:(HistoryEntryItem*)item {
  [self openURLInNewTab:item.URL];
}

- (void)historyEntryItemDidRequestOpenInNewIncognitoTab:
    (HistoryEntryItem*)item {
  [self openURLInNewIncognitoTab:item.URL];
}

- (void)historyEntryItemShouldUpdateView:(HistoryEntryItem*)item {
  NSInteger sectionIdentifier =
      [self.entryInserter sectionIdentifierForTimestamp:item.timestamp];
  // If the item is still in the model, reconfigure it.
  if ([self.tableViewModel hasSectionForSectionIdentifier:sectionIdentifier] &&
      [self.tableViewModel hasItem:item
           inSectionWithIdentifier:sectionIdentifier]) {
    [self reconfigureCellsForItems:@[ item ]];
  }
}

#pragma mark TableViewTextLinkCellDelegate

- (void)tableViewTextLinkCell:(TableViewTextLinkCell*)cell
            didRequestOpenURL:(const GURL&)URL {
  [self openURLInNewTab:URL];
}

#pragma mark UISearchResultsUpdating

- (void)updateSearchResultsForSearchController:
    (UISearchController*)searchController {
  DCHECK_EQ(self.searchController, searchController);
  NSString* text = searchController.searchBar.text;

  if (text.length == 0 && self.searchController.active) {
    [self showScrim];
  } else {
    [self hideScrim];
  }

  [self showHistoryMatchingQuery:text];
}

#pragma mark UISearchControllerDelegate

- (void)willPresentSearchController:(UISearchController*)searchController {
  [self showScrim];
}

- (void)didDismissSearchController:(UISearchController*)searchController {
  [self hideScrim];
}

#pragma mark UISearchBarDelegate

- (void)searchBarTextDidBeginEditing:(UISearchBar*)searchBar {
  self.searchInProgress = YES;
  [self updateEntriesStatusMessage];
}

- (void)searchBarTextDidEndEditing:(UISearchBar*)searchBar {
  self.searchInProgress = NO;
  [self updateEntriesStatusMessage];
}

#pragma mark UIAdaptivePresentationControllerDelegate

- (void)presentationControllerWillDismiss:
    (UIPresentationController*)presentationController {
  if (self.searchInProgress) {
    // Dismiss the keyboard if trying to dismiss the VC so the keyboard doesn't
    // linger until the VC dismissal has completed.
    [self.searchController.searchBar endEditing:YES];
  }
}

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  // Call the localDispatcher dismissHistoryWithCompletion to clean up state and
  // stop the Coordinator.
  [self.localDispatcher dismissHistoryWithCompletion:nil];
}

#pragma mark - History Data Updates

// Search history for text |query| and display the results. |query| may be nil.
// If query is empty, show all history items.
- (void)showHistoryMatchingQuery:(NSString*)query {
  self.finishedLoading = NO;
  self.currentQuery = query;
  [self fetchHistoryForQuery:query continuation:false];
}

// Deletes selected items from browser history and removes them from the
// tableView.
- (void)deleteSelectedItemsFromHistory {
  NSArray* toDeleteIndexPaths = self.tableView.indexPathsForSelectedRows;

  // Delete items from Browser History.
  std::vector<BrowsingHistoryService::HistoryEntry> entries;
  for (NSIndexPath* indexPath in toDeleteIndexPaths) {
    HistoryEntryItem* object = base::mac::ObjCCastStrict<HistoryEntryItem>(
        [self.tableViewModel itemAtIndexPath:indexPath]);
    BrowsingHistoryService::HistoryEntry entry;
    entry.url = object.URL;
    // TODO(crbug.com/634507) Remove base::TimeXXX::ToInternalValue().
    entry.all_timestamps.insert(object.timestamp.ToInternalValue());
    entries.push_back(entry);
  }
  self.historyService->RemoveVisits(entries);

  // Delete items from |self.tableView| using performBatchUpdates.
  [self.tableView performBatchUpdates:^{
    [self deleteItemsFromTableViewModelWithIndex:toDeleteIndexPaths
                        deleteItemsFromTableView:YES];
  }
      completion:^(BOOL) {
        [self updateTableViewAfterDeletingEntries];
        [self configureViewsForNonEditModeWithAnimation:YES];
      }];
  base::RecordAction(base::UserMetricsAction("HistoryPage_RemoveSelected"));
}

#pragma mark - UITableViewDelegate

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  if (section ==
      [self.tableViewModel
          sectionForSectionIdentifier:kEntriesStatusSectionIdentifier])
    return 0;
  return UITableViewAutomaticDimension;
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  if ([self.tableViewModel sectionIdentifierForSection:section] ==
      kEntriesStatusSectionIdentifier)
    return 0;
  return kSeparationSpaceBetweenSections;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ(tableView, self.tableView);
  if (self.isEditing) {
    [self updateToolbarButtons];
  } else {
    TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
    // Only navigate and record metrics if a ItemTypeHistoryEntry was selected.
    if (item.type == ItemTypeHistoryEntry) {
      if (self.searchInProgress) {
        // Set the searchController active property to NO or the SearchBar will
        // cause the navigation controller to linger for a second  when
        // dismissing.
        self.searchController.active = NO;
        base::RecordAction(
            base::UserMetricsAction("HistoryPage_SearchResultClick"));
      } else {
        base::RecordAction(
            base::UserMetricsAction("HistoryPage_EntryLinkClick"));
      }
      HistoryEntryItem* historyItem =
          base::mac::ObjCCastStrict<HistoryEntryItem>(item);
      [self openURL:historyItem.URL];
    }
  }
}

- (void)tableView:(UITableView*)tableView
    didDeselectRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ(tableView, self.tableView);
  if (self.editing)
    [self updateToolbarButtons];
}

- (BOOL)tableView:(UITableView*)tableView
    canEditRowAtIndexPath:(NSIndexPath*)indexPath {
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  return (item.type == ItemTypeHistoryEntry);
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cellToReturn =
      [super tableView:tableView cellForRowAtIndexPath:indexPath];
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  cellToReturn.userInteractionEnabled = !(item.type == ItemTypeEntriesStatus);
  if (item.type == ItemTypeHistoryEntry) {
    HistoryEntryItem* URLItem =
        base::mac::ObjCCastStrict<HistoryEntryItem>(item);
    TableViewURLCell* URLCell =
        base::mac::ObjCCastStrict<TableViewURLCell>(cellToReturn);
    [self.imageDataSource
        faviconForURL:URLItem.URL
           completion:^(FaviconAttributes* attributes) {
             // Only set favicon if the cell hasn't been reused.
             if ([URLCell.cellUniqueIdentifier
                     isEqualToString:URLItem.uniqueIdentifier]) {
               DCHECK(attributes);
               [URLCell.faviconView configureWithAttributes:attributes];
             }
           }];
  }
  if (item.type == ItemTypeEntriesStatusWithLink) {
    TableViewTextLinkCell* tableViewTextLinkCell =
        base::mac::ObjCCastStrict<TableViewTextLinkCell>(cellToReturn);
    [tableViewTextLinkCell setDelegate:self];
  }
  return cellToReturn;
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  [super scrollViewDidScroll:scrollView];

  if (self.hasFinishedLoading)
    return;

  CGFloat insetHeight =
      scrollView.contentInset.top + scrollView.contentInset.bottom;
  CGFloat contentViewHeight = scrollView.bounds.size.height - insetHeight;
  CGFloat contentHeight = scrollView.contentSize.height;
  CGFloat contentOffset = scrollView.contentOffset.y;
  CGFloat buffer = contentViewHeight;
  // If the scroll view is approaching the end of loaded history, try to fetch
  // more history. Do so when the content offset is greater than the content
  // height minus the view height, minus a buffer to start the fetch early.
  if (contentOffset > (contentHeight - contentViewHeight) - buffer &&
      !self.isLoading) {
    // If at end, try to grab more history.
    NSInteger lastSection = [self.tableViewModel numberOfSections] - 1;
    NSInteger lastItemIndex =
        [self.tableViewModel numberOfItemsInSection:lastSection] - 1;
    if (lastSection == 0 || lastItemIndex < 0) {
      return;
    }

    [self fetchHistoryForQuery:_currentQuery continuation:true];
  }
}

#pragma mark - Private methods

// Fetches history for search text |query|. If |query| is nil or the empty
// string, all history is fetched. If continuation is false, then the most
// recent results are fetched, otherwise the results more recent than the
// previous query will be returned.
- (void)fetchHistoryForQuery:(NSString*)query continuation:(BOOL)continuation {
  self.loading = YES;
  // Add loading indicator if no items are shown.
  if (self.empty && !self.searchInProgress) {
    [self startLoadingIndicatorWithLoadingMessage:l10n_util::GetNSString(
                                                      IDS_HISTORY_NO_RESULTS)];
  }

  if (continuation) {
    DCHECK(_query_history_continuation);
    std::move(_query_history_continuation).Run();
  } else {
    _query_history_continuation.Reset();

    BOOL fetchAllHistory = !query || [query isEqualToString:@""];
    base::string16 queryString =
        fetchAllHistory ? base::string16() : base::SysNSStringToUTF16(query);
    history::QueryOptions options;
    options.duplicate_policy =
        fetchAllHistory ? history::QueryOptions::REMOVE_DUPLICATES_PER_DAY
                        : history::QueryOptions::REMOVE_ALL_DUPLICATES;
    options.max_count = kMaxFetchCount;
    options.matching_algorithm =
        query_parser::MatchingAlgorithm::ALWAYS_PREFIX_SEARCH;
    self.historyService->QueryHistory(queryString, options);
  }
}

// Updates various elements after history items have been deleted from the
// TableView.
- (void)updateTableViewAfterDeletingEntries {
  // If only the header section remains, there are no history entries.
  if ([self.tableViewModel numberOfSections] == 1) {
    self.empty = YES;
    if (!self.searchInProgress) {
      UIImage* emptyImage = [[UIImage imageNamed:@"empty_history"]
          imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
      [self addEmptyTableViewWithMessage:l10n_util::GetNSString(
                                             IDS_HISTORY_NO_RESULTS)
                                   image:emptyImage];
    }
  }
  [self updateEntriesStatusMessage];
  [self updateToolbarButtons];
}

// Updates header section to provide relevant information about the currently
// displayed history entries. There should only ever be at most one item in this
// section.
- (void)updateEntriesStatusMessage {
  // Get the new status message, newStatusMessage could be nil.
  NSString* newStatusMessage = nil;
  BOOL messageWillContainLink = NO;
  if (self.empty) {
    newStatusMessage =
        self.searchController.isActive
            ? l10n_util::GetNSString(IDS_HISTORY_NO_SEARCH_RESULTS)
            : nil;
  } else if (self.shouldShowNoticeAboutOtherFormsOfBrowsingHistory &&
             !self.searchController.isActive) {
    newStatusMessage =
        l10n_util::GetNSString(IDS_IOS_HISTORY_OTHER_FORMS_OF_HISTORY);
    messageWillContainLink = YES;
  }

  // If the new message is the same as the old one, there's no need to do
  // anything else. Compare the objects since they might both be nil.
  if ([self.currentStatusMessage isEqualToString:newStatusMessage] ||
      newStatusMessage == self.currentStatusMessage)
    return;

  // Get the previous status item and its information, if any.
  NSArray* previousStatusItems = [self.tableViewModel
      itemsInSectionWithIdentifier:kEntriesStatusSectionIdentifier];
  DCHECK([previousStatusItems count] <= 1);
  TableViewItem* previousStatusItem = nil;
  NSIndexPath* previousStatusItemIndexPath = nil;
  if ([previousStatusItems count]) {
    previousStatusItem = [previousStatusItems lastObject];
    previousStatusItemIndexPath = [self.tableViewModel
        indexPathForItemType:previousStatusItem.type
           sectionIdentifier:kEntriesStatusSectionIdentifier];
  }

  // Block to hold any tableView and model updates that will be performed.
  void (^tableUpdates)(void) = nil;

  // If no new status message remove the previous status item if it exists.
  if (newStatusMessage == nil) {
    if (previousStatusItem) {
      tableUpdates = ^{
        [self.tableViewModel
                   removeItemWithType:previousStatusItem.type
            fromSectionWithIdentifier:kEntriesStatusSectionIdentifier];
        [self.tableView
            deleteRowsAtIndexPaths:@[ previousStatusItemIndexPath ]
                  withRowAnimation:UITableViewRowAnimationAutomatic];
      };
    }
  } else {
    // Since there's a new status message, create the new status item.
    TableViewItem* updatedMessageItem =
        [self statusItemWithMessage:newStatusMessage
             messageWillContainLink:messageWillContainLink];

    // If there was a previous status item delete it, insert the new status item
    // and reload. If not simply insert the new status item.
    tableUpdates = ^{
      if (previousStatusItem) {
        [self.tableViewModel
                   removeItemWithType:previousStatusItem.type
            fromSectionWithIdentifier:kEntriesStatusSectionIdentifier];
        [self.tableViewModel addItem:updatedMessageItem
             toSectionWithIdentifier:kEntriesStatusSectionIdentifier];
        [self.tableView
            reloadRowsAtIndexPaths:@[ [self.tableViewModel
                                       indexPathForItem:updatedMessageItem] ]
                  withRowAnimation:UITableViewRowAnimationAutomatic];
      } else {
        [self.tableViewModel addItem:updatedMessageItem
             toSectionWithIdentifier:kEntriesStatusSectionIdentifier];
        [self.tableView
            insertRowsAtIndexPaths:@[ [self.tableViewModel
                                       indexPathForItem:updatedMessageItem] ]
                  withRowAnimation:UITableViewRowAnimationAutomatic];
      }
    };
  }

  // If there's any tableUpdates, run them.
  if (tableUpdates) {
    [self.tableView performBatchUpdates:tableUpdates completion:nil];
  }
  self.currentStatusMessage = newStatusMessage;
}

// Helper function that creates a new item for the Status message.
- (TableViewItem*)statusItemWithMessage:(NSString*)statusMessage
                 messageWillContainLink:(BOOL)messageWillContainLink {
  TableViewItem* statusMessageItem = nil;
  if (messageWillContainLink) {
    TableViewTextLinkItem* entriesStatusItem = [[TableViewTextLinkItem alloc]
        initWithType:ItemTypeEntriesStatusWithLink];
    entriesStatusItem.text = statusMessage;
    entriesStatusItem.linkURL = GURL(kHistoryMyActivityURL);
    statusMessageItem = entriesStatusItem;
  } else {
    TableViewTextItem* entriesStatusItem =
        [[TableViewTextItem alloc] initWithType:ItemTypeEntriesStatus];
    entriesStatusItem.text = statusMessage;
    entriesStatusItem.textColor = UIColor.cr_labelColor;
    statusMessageItem = entriesStatusItem;
  }
  return statusMessageItem;
}

// Deletes all items in the tableView which indexes are included in indexArray,
// if |deleteItemsFromTableView| is YES this method needs to be run inside a
// performBatchUpdates block.
- (void)deleteItemsFromTableViewModelWithIndex:(NSArray*)indexArray
                      deleteItemsFromTableView:(BOOL)deleteItemsFromTableView {
  NSArray* sortedIndexPaths =
      [indexArray sortedArrayUsingSelector:@selector(compare:)];
  for (NSIndexPath* indexPath in [sortedIndexPaths reverseObjectEnumerator]) {
    NSInteger sectionIdentifier =
        [self.tableViewModel sectionIdentifierForSection:indexPath.section];
    NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
    NSUInteger index =
        [self.tableViewModel indexInItemTypeForIndexPath:indexPath];
    [self.tableViewModel removeItemWithType:itemType
                  fromSectionWithIdentifier:sectionIdentifier
                                    atIndex:index];
  }
  if (deleteItemsFromTableView)
    [self.tableView deleteRowsAtIndexPaths:indexArray
                          withRowAnimation:UITableViewRowAnimationNone];

  // Remove any empty sections, except the header section.
  for (int section = self.tableView.numberOfSections - 1; section > 0;
       --section) {
    if (![self.tableViewModel numberOfItemsInSection:section]) {
      [self.entryInserter removeSection:section];
      if (deleteItemsFromTableView)
        [self.tableView deleteSections:[NSIndexSet indexSetWithIndex:section]
                      withRowAnimation:UITableViewRowAnimationAutomatic];
    }
  }
}

// Selects all items in the tableView that are not included in entries.
- (void)filterForHistoryEntries:(NSArray*)entries {
  for (int section = 1; section < [self.tableViewModel numberOfSections];
       ++section) {
    NSInteger sectionIdentifier =
        [self.tableViewModel sectionIdentifierForSection:section];
    if ([self.tableViewModel
            hasSectionForSectionIdentifier:sectionIdentifier]) {
      NSArray* items =
          [self.tableViewModel itemsInSectionWithIdentifier:sectionIdentifier];
      for (id item in items) {
        HistoryEntryItem* historyItem =
            base::mac::ObjCCastStrict<HistoryEntryItem>(item);
        if (![entries containsObject:historyItem]) {
          NSIndexPath* indexPath =
              [self.tableViewModel indexPathForItem:historyItem];
          [self.filteredOutEntriesIndexPaths addObject:indexPath];
        }
      }
    }
  }
}

// Dismisses the search controller when there's a touch event on the scrim.
- (void)dismissSearchController:(UIControl*)sender {
  if (self.searchController.active) {
    self.searchController.active = NO;
  }
}

// Shows scrim overlay and hide toolbar.
- (void)showScrim {
  if (self.scrimView.alpha < 1.0f) {
    self.navigationController.toolbarHidden = YES;
    self.scrimView.alpha = 0.0f;
    [self.tableView addSubview:self.scrimView];
    // We attach our constraints to the superview because the tableView is
    // a scrollView and it seems that we get an empty frame when attaching to
    // it.
    AddSameConstraints(self.scrimView, self.view.superview);
    self.tableView.accessibilityElementsHidden = YES;
    self.tableView.scrollEnabled = NO;
    [UIView animateWithDuration:kTableViewNavigationScrimFadeDuration
                     animations:^{
                       self.scrimView.alpha = 1.0f;
                       [self.view layoutIfNeeded];
                     }];
  }
}

// Hides scrim and restore toolbar.
- (void)hideScrim {
  if (self.scrimView.alpha > 0.0f) {
    self.navigationController.toolbarHidden = NO;
    [UIView animateWithDuration:kTableViewNavigationScrimFadeDuration
        animations:^{
          self.scrimView.alpha = 0.0f;
        }
        completion:^(BOOL finished) {
          [self.scrimView removeFromSuperview];
          self.tableView.accessibilityElementsHidden = NO;
          self.tableView.scrollEnabled = YES;
        }];
  }
}

- (BOOL)scrimIsVisible {
  return self.scrimView.superview ? YES : NO;
}

#pragma mark Navigation Toolbar Configuration

// Animates the view configuration after flipping the current status of |[self
// setEditing]|.
- (void)animateViewsConfigurationForEditingChange {
  if (self.isEditing) {
    [self configureViewsForNonEditModeWithAnimation:YES];
  } else {
    [self configureViewsForEditModeWithAnimation:YES];
  }
}

// Default TableView and NavigationBar UIToolbar configuration.
- (void)configureViewsForNonEditModeWithAnimation:(BOOL)animated {
  [self setEditing:NO animated:animated];
  UIBarButtonItem* spaceButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace
                           target:nil
                           action:nil];
  [self setToolbarItems:@[
    self.clearBrowsingDataButton, spaceButton, self.editButton
  ]
               animated:animated];
  [self.searchController.searchBar setUserInteractionEnabled:YES];
  self.searchController.searchBar.alpha = 1.0;
  [self updateToolbarButtons];
}

// Configures the TableView and NavigationBar UIToolbar for edit mode.
- (void)configureViewsForEditModeWithAnimation:(BOOL)animated {
  [self setEditing:YES animated:animated];
  UIBarButtonItem* spaceButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace
                           target:nil
                           action:nil];
  [self setToolbarItems:@[ self.deleteButton, spaceButton, self.cancelButton ]
               animated:animated];
  [self.searchController.searchBar setUserInteractionEnabled:NO];
  self.searchController.searchBar.alpha =
      kTableViewNavigationAlphaForDisabledSearchBar;
  [self updateToolbarButtons];
}

// Updates the NavigationBar UIToolbar buttons.
- (void)updateToolbarButtons {
  self.deleteButton.enabled =
      [[self.tableView indexPathsForSelectedRows] count];
  self.editButton.enabled = !self.empty;
}

#pragma mark Context Menu

// Displays context menu on cell pressed with gestureRecognizer.
- (void)displayContextMenuInvokedByGestureRecognizer:
    (UILongPressGestureRecognizer*)gestureRecognizer {
  if (gestureRecognizer.numberOfTouches != 1 || self.editing ||
      gestureRecognizer.state != UIGestureRecognizerStateBegan) {
    return;
  }
  if ([self scrimIsVisible]) {
    self.searchController.active = NO;
    return;
  }

  CGPoint touchLocation =
      [gestureRecognizer locationOfTouch:0 inView:self.tableView];
  NSIndexPath* touchedItemIndexPath =
      [self.tableView indexPathForRowAtPoint:touchLocation];
  // If there's no index path, or the index path is for the header item, do not
  // display a contextual menu.
  if (!touchedItemIndexPath ||
      [touchedItemIndexPath
          isEqual:[NSIndexPath indexPathForItem:0 inSection:0]])
    return;

  HistoryEntryItem* entry = base::mac::ObjCCastStrict<HistoryEntryItem>(
      [self.tableViewModel itemAtIndexPath:touchedItemIndexPath]);

  __weak HistoryTableViewController* weakSelf = self;
  NSString* menuTitle =
      base::SysUTF16ToNSString(url_formatter::FormatUrl(entry.URL));
  self.contextMenuCoordinator = [[ContextMenuCoordinator alloc]
      initWithBaseViewController:self.navigationController
                           title:menuTitle
                          inView:self.tableView
                      atLocation:touchLocation];

  // Add "Open in New Tab" option.
  NSString* openInNewTabTitle =
      l10n_util::GetNSStringWithFixup(IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB);
  ProceduralBlock openInNewTabAction = ^{
    [weakSelf openURLInNewTab:entry.URL];
  };
  [self.contextMenuCoordinator addItemWithTitle:openInNewTabTitle
                                         action:openInNewTabAction];

  // Add "Open in New Incognito Tab" option.
  NSString* openInNewIncognitoTabTitle = l10n_util::GetNSStringWithFixup(
      IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWINCOGNITOTAB);
  ProceduralBlock openInNewIncognitoTabAction = ^{
    [weakSelf openURLInNewIncognitoTab:entry.URL];
  };
  [self.contextMenuCoordinator addItemWithTitle:openInNewIncognitoTabTitle
                                         action:openInNewIncognitoTabAction];

  // Add "Copy URL" option.
  NSString* copyURLTitle =
      l10n_util::GetNSStringWithFixup(IDS_IOS_CONTENT_CONTEXT_COPY);
  ProceduralBlock copyURLAction = ^{
    StoreURLInPasteboard(entry.URL);
  };
  [self.contextMenuCoordinator addItemWithTitle:copyURLTitle
                                         action:copyURLAction];
  [self.contextMenuCoordinator start];
}

// Opens URL in a new non-incognito tab and dismisses the history view.
- (void)openURLInNewTab:(const GURL&)URL {
  base::RecordAction(
      base::UserMetricsAction("MobileHistoryPage_EntryLinkOpenNewTab"));
  UrlLoadParams params = UrlLoadParams::InNewTab(URL);
  [self.localDispatcher dismissHistoryWithCompletion:^{
    UrlLoadingServiceFactory::GetForBrowserState(_browserState)->Load(params);
    [self.presentationDelegate showActiveRegularTabFromHistory];
  }];
}

// Opens URL in a new incognito tab and dismisses the history view.
- (void)openURLInNewIncognitoTab:(const GURL&)URL {
  base::RecordAction(base::UserMetricsAction(
      "MobileHistoryPage_EntryLinkOpenNewIncognitoTab"));
  UrlLoadParams params = UrlLoadParams::InNewTab(URL);
  params.in_incognito = YES;
  [self.localDispatcher dismissHistoryWithCompletion:^{
    UrlLoadingServiceFactory::GetForBrowserState(_browserState)->Load(params);
    [self.presentationDelegate showActiveIncognitoTabFromHistory];
  }];
}

#pragma mark Helper Methods

// Opens URL in the current tab and dismisses the history view.
- (void)openURL:(const GURL&)URL {
  new_tab_page_uma::RecordAction(_browserState,
                                 new_tab_page_uma::ACTION_OPENED_HISTORY_ENTRY);
  UrlLoadParams params = UrlLoadParams::InCurrentTab(URL);
  params.web_params.transition_type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
  params.load_strategy = self.loadStrategy;
  [self.localDispatcher dismissHistoryWithCompletion:^{
    UrlLoadingServiceFactory::GetForBrowserState(_browserState)->Load(params);
    [self.presentationDelegate showActiveRegularTabFromHistory];
  }];
}

// Dismisses this ViewController.
- (void)dismissHistory {
  [self.localDispatcher dismissHistoryWithCompletion:nil];
}

- (void)openPrivacySettings {
  base::RecordAction(
      base::UserMetricsAction("HistoryPage_InitClearBrowsingData"));
  [self.localDispatcher displayPrivacySettings];
}

#pragma mark - Accessibility

- (BOOL)accessibilityPerformEscape {
  [self.localDispatcher dismissHistoryWithCompletion:nil];
  return YES;
}

#pragma mark Setter & Getters

- (UIBarButtonItem*)cancelButton {
  if (!_cancelButton) {
    NSString* titleString =
        l10n_util::GetNSString(IDS_HISTORY_CANCEL_EDITING_BUTTON);
    _cancelButton = [[UIBarButtonItem alloc]
        initWithTitle:titleString
                style:UIBarButtonItemStylePlain
               target:self
               action:@selector(animateViewsConfigurationForEditingChange)];
    _cancelButton.accessibilityIdentifier =
        kHistoryToolbarCancelButtonIdentifier;
  }
  return _cancelButton;
}

// TODO(crbug.com/831865): Find a way to disable the button when a VC is
// presented.
- (UIBarButtonItem*)clearBrowsingDataButton {
  if (!_clearBrowsingDataButton) {
    NSString* titleString = l10n_util::GetNSStringWithFixup(
        IDS_HISTORY_OPEN_CLEAR_BROWSING_DATA_DIALOG);
    _clearBrowsingDataButton =
        [[UIBarButtonItem alloc] initWithTitle:titleString
                                         style:UIBarButtonItemStylePlain
                                        target:self
                                        action:@selector(openPrivacySettings)];
    _clearBrowsingDataButton.accessibilityIdentifier =
        kHistoryToolbarClearBrowsingButtonIdentifier;
    _clearBrowsingDataButton.tintColor = [UIColor colorNamed:kRedColor];
  }
  return _clearBrowsingDataButton;
}

- (UIBarButtonItem*)deleteButton {
  if (!_deleteButton) {
    NSString* titleString =
        l10n_util::GetNSString(IDS_HISTORY_DELETE_SELECTED_ENTRIES_BUTTON);
    _deleteButton = [[UIBarButtonItem alloc]
        initWithTitle:titleString
                style:UIBarButtonItemStylePlain
               target:self
               action:@selector(deleteSelectedItemsFromHistory)];
    _deleteButton.accessibilityIdentifier =
        kHistoryToolbarDeleteButtonIdentifier;
    _deleteButton.tintColor = [UIColor colorNamed:kRedColor];
  }
  return _deleteButton;
}

- (UIBarButtonItem*)editButton {
  if (!_editButton) {
    NSString* titleString =
        l10n_util::GetNSString(IDS_HISTORY_START_EDITING_BUTTON);
    _editButton = [[UIBarButtonItem alloc]
        initWithTitle:titleString
                style:UIBarButtonItemStylePlain
               target:self
               action:@selector(animateViewsConfigurationForEditingChange)];
    _editButton.accessibilityIdentifier = kHistoryToolbarEditButtonIdentifier;
    // Buttons don't conform to dynamic types. So it's safe to just use the
    // default font size.
    CGSize stringSize = [titleString sizeWithAttributes:@{
      NSFontAttributeName : [UIFont boldSystemFontOfSize:kButtonDefaultFontSize]
    }];
    // Include button padding to ensure string does not get truncated
    _editButton.width = stringSize.width + kButtonHorizontalPadding;
  }
  return _editButton;
}

- (NSMutableArray<NSIndexPath*>*)filteredOutEntriesIndexPaths {
  if (!_filteredOutEntriesIndexPaths)
    _filteredOutEntriesIndexPaths = [[NSMutableArray alloc] init];
  return _filteredOutEntriesIndexPaths;
}

@end
