// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/history/history_table_view_controller.h"

#import "base/i18n/time_formatting.h"
#import "base/ios/ios_util.h"
#import "base/mac/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/base/model_type.h"
#import "components/sync/service/sync_service.h"
#import "components/url_formatter/elide_url.h"
#import "components/url_formatter/url_formatter.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/drag_and_drop/drag_item_util.h"
#import "ios/chrome/browser/drag_and_drop/table_view_url_drag_drop_handler.h"
#import "ios/chrome/browser/metrics/new_tab_page_uma.h"
#import "ios/chrome/browser/net/crurl.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_link_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_favicon_data_source.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller_constants.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/pasteboard_util.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/ui/history/history_entries_status_item.h"
#import "ios/chrome/browser/ui/history/history_entries_status_item_delegate.h"
#import "ios/chrome/browser/ui/history/history_entry_inserter.h"
#import "ios/chrome/browser/ui/history/history_entry_item.h"
#import "ios/chrome/browser/ui/history/history_menu_provider.h"
#import "ios/chrome/browser/ui/history/history_table_view_controller_delegate.h"
#import "ios/chrome/browser/ui/history/history_ui_constants.h"
#import "ios/chrome/browser/ui/history/history_util.h"
#import "ios/chrome/browser/ui/history/public/history_presentation_delegate.h"
#import "ios/chrome/browser/ui/keyboard/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/browser/window_activities/window_activity_helpers.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/referrer.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/strings/grit/ui_strings.h"

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

@interface HistoryTableViewController () <HistoryEntriesStatusItemDelegate,
                                          HistoryEntryInserterDelegate,
                                          TableViewLinkHeaderFooterItemDelegate,
                                          TableViewURLDragDataSource,
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
// Handler for URL drag interactions.
@property(nonatomic, strong) TableViewURLDragDropHandler* dragDropHandler;
@end

@implementation HistoryTableViewController

#pragma mark - ViewController Lifecycle.

- (instancetype)init {
  UITableViewStyle style = ChromeTableViewStyle();
  return [super initWithStyle:style];
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
  self.tableView.accessibilityIdentifier = kHistoryTableViewIdentifier;
  // Add a tableFooterView in order to hide the separator lines where there's no
  // history content.
  self.tableView.tableFooterView = [[UIView alloc] init];

  self.dragDropHandler = [[TableViewURLDragDropHandler alloc] init];
  self.dragDropHandler.origin = WindowActivityHistoryOrigin;
  self.dragDropHandler.dragDataSource = self;
  self.tableView.dragDelegate = self.dragDropHandler;
  self.tableView.dragInteractionEnabled = true;

  // NavigationController configuration.
  self.title = l10n_util::GetNSString(IDS_HISTORY_TITLE);
  // Configures NavigationController Toolbar buttons.
  [self configureViewsForNonEditModeWithAnimation:NO];
  // Adds the "Done" button and hooks it up to `dismissHistory`.
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
  if (self.searchTerms.length) {
    self.searchController.searchBar.text = self.searchTerms;
    self.searchInProgress = YES;
  }
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
  [self updateNavigationBar];
  self.navigationItem.hidesSearchBarWhenScrolling = NO;
}

- (void)detachFromBrowser {
  // Clear C++ ivars.
  _browser = nullptr;
  _historyService = nullptr;
  [self dismissContextMenuCoordinator];
}

#pragma mark - TableViewModel

- (void)loadModel {
  [super loadModel];
  // Add Status section, this section will always exist during the lifetime of
  // HistoryTableVC. Its content will be driven by `updateEntriesStatusMessage`.
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
  if (!self.browser)
    return;

  self.loading = NO;
  _query_history_continuation = std::move(continuationClosure);

  // If history sync is enabled and there hasn't been a response from synced
  // history, try fetching again.
  syncer::SyncService* syncService =
      SyncServiceFactory::GetForBrowserState(self.browser->GetBrowserState());
  if (syncService->GetActiveDataTypes().Has(
          syncer::HISTORY_DELETE_DIRECTIVES) &&
      queryResultsInfo.sync_timed_out) {
    [self showHistoryMatchingQuery:_currentQuery];
    return;
  }

  // At this point there has been a response, stop the loading indicator.
  [self stopLoadingIndicatorWithCompletion:nil];

  // If there are no results and no URLs have been loaded, report that no
  // history entries were found.
  if (results.empty() && self.empty && !self.searchInProgress) {
    [self addEmptyTableViewBackground];
    [self updateToolbarButtonsWithAnimation:NO];
    return;
  }

  self.finishedLoading = queryResultsInfo.reached_beginning;
  self.empty = NO;
  [self removeEmptyTableViewBackground];

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
    item.detailText = base::SysUTF16ToNSString(
        url_formatter::
            FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                entry.url));
    item.timeText =
        [base::SysUTF16ToNSString(base::TimeFormatTimeOfDay(entry.time)) copy];
    item.URL = entry.url;
    item.timestamp = entry.time;
    [resultsItems addObject:item];
  }

  [self updateToolbarButtonsWithAnimation:YES];

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

#pragma mark TableViewLinkHeaderFooterItemDelegate

- (void)view:(TableViewLinkHeaderFooterView*)view didTapLinkURL:(CrURL*)URL {
  [self openURLInNewTab:URL.gurl];
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

  if (text.length != 0) {
    self.searchInProgress = YES;
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
  base::RecordAction(base::UserMetricsAction("IOSHistoryCloseWithSwipe"));
  // Call the delegate dismissHistoryTableViewController:withCompletion: to
  // clean up state and stop the Coordinator.
  [self.delegate dismissHistoryTableViewController:self withCompletion:nil];
}

#pragma mark - History Data Updates

// Search history for text `query` and display the results. `query` may be nil.
// If query is empty, show all history items.
- (void)showHistoryMatchingQuery:(NSString*)query {
  self.finishedLoading = NO;
  self.currentQuery = query;
  [self fetchHistoryForQuery:query continuation:false];
}

// Deletes selected items from browser history and removes them from the
// tableView.
- (void)deleteSelectedItemsFromHistory {
  if (!self.browser)
    return;

  if (!self.historyService)
    return;

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

  // Delete items from `self.tableView` using performBatchUpdates.
  __weak __typeof(self) weakSelf = self;
  [self.tableView
      performBatchUpdates:^{
        [weakSelf deleteItemsFromTableViewModelWithIndex:toDeleteIndexPaths
                                deleteItemsFromTableView:YES];
      }
      completion:^(BOOL) {
        [weakSelf updateTableViewAfterDeletingEntries];
        [weakSelf configureViewsForNonEditModeWithAnimation:YES];
      }];
  base::RecordAction(base::UserMetricsAction("HistoryPage_RemoveSelected"));
}

#pragma mark - UITableViewDelegate

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  if ([self.tableViewModel sectionIdentifierForSectionIndex:section] ==
      kEntriesStatusSectionIdentifier)
    return 0;
  return kSeparationSpaceBetweenSections;
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  // Hide the status header if the currentStatusMessage is nil.
  if ([self.tableViewModel sectionIdentifierForSectionIndex:section] ==
          kEntriesStatusSectionIdentifier &&
      [self.currentStatusMessage length] == 0)
    return 0;
  return UITableViewAutomaticDimension;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ(tableView, self.tableView);
  if (self.isEditing) {
    [self updateToolbarButtonsWithAnimation:YES];
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
    [self updateToolbarButtonsWithAnimation:YES];
}

- (BOOL)tableView:(UITableView*)tableView
    canEditRowAtIndexPath:(NSIndexPath*)indexPath {
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  return (item.type == ItemTypeHistoryEntry);
}

- (UIContextMenuConfiguration*)tableView:(UITableView*)tableView
    contextMenuConfigurationForRowAtIndexPath:(NSIndexPath*)indexPath
                                        point:(CGPoint)point {
  if (self.isEditing) {
    // Don't show the context menu when currently in editing mode.
    return nil;
  }

  if (indexPath.section ==
      [self.tableViewModel
          sectionForSectionIdentifier:kEntriesStatusSectionIdentifier]) {
    return nil;
  }

  HistoryEntryItem* entry = base::mac::ObjCCastStrict<HistoryEntryItem>(
      [self.tableViewModel itemAtIndexPath:indexPath]);
  UIView* cell = [self.tableView cellForRowAtIndexPath:indexPath];
  return [self.menuProvider contextMenuConfigurationForItem:entry
                                                   withView:cell];
}

#pragma mark - UITableViewDataSource

- (UIView*)tableView:(UITableView*)tableView
    viewForHeaderInSection:(NSInteger)section {
  UIView* view = [super tableView:tableView viewForHeaderInSection:section];
  NSInteger sectionIdentifier =
      [self.tableViewModel sectionIdentifierForSectionIndex:section];
  switch (sectionIdentifier) {
    case kEntriesStatusSectionIdentifier: {
      // Might be a different type of header.
      TableViewLinkHeaderFooterView* linkView =
          base::mac::ObjCCast<TableViewLinkHeaderFooterView>(view);
      linkView.delegate = self;
    } break;
    default:
      break;
  }
  return view;
}

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
    CrURL* crurl = [[CrURL alloc] initWithGURL:URLItem.URL];
    [self.imageDataSource
        faviconForPageURL:crurl
               completion:^(FaviconAttributes* attributes) {
                 // Only set favicon if the cell hasn't been reused.
                 if ([URLCell.cellUniqueIdentifier
                         isEqualToString:URLItem.uniqueIdentifier]) {
                   DCHECK(attributes);
                   [URLCell.faviconView configureWithAttributes:attributes];
                 }
               }];
  }
  return cellToReturn;
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {

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

#pragma mark - UIResponder

// To always be able to register key commands via -keyCommands, the VC must be
// able to become first responder.
- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (NSArray*)keyCommands {
  return @[ UIKeyCommand.cr_close ];
}

- (void)keyCommand_close {
  base::RecordAction(base::UserMetricsAction("MobileKeyCommandClose"));
  [self.delegate dismissHistoryTableViewController:self withCompletion:nil];
}

#pragma mark - TableViewURLDragDataSource

- (URLInfo*)tableView:(UITableView*)tableView
    URLInfoAtIndexPath:(NSIndexPath*)indexPath {
  if (self.tableView.isEditing)
    return nil;

  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  switch (item.type) {
    case ItemTypeHistoryEntry: {
      HistoryEntryItem* URLItem =
          base::mac::ObjCCastStrict<HistoryEntryItem>(item);
      return [[URLInfo alloc] initWithURL:URLItem.URL title:URLItem.text];
    }
    case ItemTypeEntriesStatus:
    case ItemTypeActivityIndicator:
    case ItemTypeEntriesStatusWithLink:
      break;
  }
  return nil;
}

#pragma mark - Private methods

- (void)dismissContextMenuCoordinator {
  [self.contextMenuCoordinator stop];
  self.contextMenuCoordinator = nil;
}

// Fetches history for search text `query`. If `query` is nil or the empty
// string, all history is fetched. If continuation is false, then the most
// recent results are fetched, otherwise the results more recent than the
// previous query will be returned.
- (void)fetchHistoryForQuery:(NSString*)query continuation:(BOOL)continuation {
  if (!self.browser)
    return;

  if (!self.historyService)
    return;

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
    std::u16string queryString =
        fetchAllHistory ? std::u16string() : base::SysNSStringToUTF16(query);
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
      [self addEmptyTableViewBackground];
    }
  }
  [self updateEntriesStatusMessage];
  [self updateToolbarButtonsWithAnimation:YES];
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

  self.currentStatusMessage = newStatusMessage;

  TableViewHeaderFooterItem* item = nil;
  if (messageWillContainLink) {
    TableViewLinkHeaderFooterItem* header =
        [[TableViewLinkHeaderFooterItem alloc]
            initWithType:ItemTypeEntriesStatusWithLink];
    header.text = newStatusMessage;
    header.urls = @[ [[CrURL alloc] initWithGURL:GURL(kHistoryMyActivityURL)] ];
    item = header;
  } else {
    TableViewTextHeaderFooterItem* header =
        [[TableViewTextHeaderFooterItem alloc]
            initWithType:ItemTypeEntriesStatus];
    header.text = newStatusMessage;
    item = header;
  }

  // Block to hold any tableView and model updates that will be performed.
  // Change the header then reload the section to have it taken into
  // account.
  void (^tableUpdates)(void) = ^{
    [self.tableViewModel setHeader:item
          forSectionWithIdentifier:kEntriesStatusSectionIdentifier];
    NSInteger sectionIndex = [self.tableViewModel
        sectionForSectionIdentifier:kEntriesStatusSectionIdentifier];
    [self.tableView reloadSections:[NSIndexSet indexSetWithIndex:sectionIndex]
                  withRowAnimation:UITableViewRowAnimationAutomatic];
  };
  [self.tableView performBatchUpdates:tableUpdates completion:nil];
}

// Deletes all items in the tableView which indexes are included in indexArray,
// if `deleteItemsFromTableView` is YES this method needs to be run inside a
// performBatchUpdates block.
- (void)deleteItemsFromTableViewModelWithIndex:(NSArray*)indexArray
                      deleteItemsFromTableView:(BOOL)deleteItemsFromTableView {
  NSArray* sortedIndexPaths =
      [indexArray sortedArrayUsingSelector:@selector(compare:)];
  for (NSIndexPath* indexPath in [sortedIndexPaths reverseObjectEnumerator]) {
    NSInteger sectionIdentifier = [self.tableViewModel
        sectionIdentifierForSectionIndex:indexPath.section];
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
        [self.tableViewModel sectionIdentifierForSectionIndex:section];
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
    __weak __typeof(self) weakSelf = self;
    [UIView animateWithDuration:kTableViewNavigationScrimFadeDuration
                     animations:^{
                       weakSelf.scrimView.alpha = 1.0f;
                       [weakSelf.view layoutIfNeeded];
                     }];
  }
}

// Hides scrim and restore toolbar.
- (void)hideScrim {
  if (self.scrimView.alpha > 0.0f) {
    self.navigationController.toolbarHidden = NO;
    __weak __typeof(self) weakSelf = self;
    [UIView animateWithDuration:kTableViewNavigationScrimFadeDuration
        animations:^{
          weakSelf.scrimView.alpha = 0.0f;
        }
        completion:^(BOOL finished) {
          [weakSelf.scrimView removeFromSuperview];
          weakSelf.tableView.accessibilityElementsHidden = NO;
          weakSelf.tableView.scrollEnabled = YES;
        }];
  }
}

- (BOOL)scrimIsVisible {
  return self.scrimView.superview ? YES : NO;
}

#pragma mark Navigation Toolbar Configuration

// Animates the view configuration after flipping the current status of `[self
// setEditing]`.
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

  [self.searchController.searchBar setUserInteractionEnabled:YES];
  self.searchController.searchBar.alpha = 1.0;
  [self updateToolbarButtonsWithAnimation:animated];
}

// Configures the TableView and NavigationBar UIToolbar for edit mode.
- (void)configureViewsForEditModeWithAnimation:(BOOL)animated {
  [self setEditing:YES animated:animated];
  [self.searchController.searchBar setUserInteractionEnabled:NO];
  self.searchController.searchBar.alpha =
      kTableViewNavigationAlphaForDisabledSearchBar;
  [self updateToolbarButtonsWithAnimation:animated];
}

// Updates the NavigationBar UIToolbar buttons.
- (void)updateToolbarButtonsWithAnimation:(BOOL)animated {
  self.deleteButton.enabled =
      [[self.tableView indexPathsForSelectedRows] count];
  self.editButton.enabled = !self.empty;
  [self setToolbarItems:[self toolbarButtons] animated:animated];
}

// Configure the navigationItem contents for the current state.
- (void)updateNavigationBar {
  if ([self isEmptyState]) {
    self.navigationItem.searchController = nil;
    self.navigationItem.largeTitleDisplayMode =
        UINavigationItemLargeTitleDisplayModeNever;
  } else {
    self.navigationItem.searchController = self.searchController;
    self.navigationItem.largeTitleDisplayMode =
        UINavigationItemLargeTitleDisplayModeAutomatic;
  }
}

#pragma mark Context Menu

// Displays a context menu on the cell pressed with gestureRecognizer.
- (void)displayContextMenuInvokedByGestureRecognizer:
    (UILongPressGestureRecognizer*)gestureRecognizer {
  if (!self.browser) {
    return;
  }
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
  self.contextMenuCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:self.navigationController
                         browser:self.browser
                           title:menuTitle
                         message:nil
                            rect:CGRectMake(touchLocation.x, touchLocation.y,
                                            1.0, 1.0)
                            view:self.tableView];

  // Add "Open in New Tab" option.
  NSString* openInNewTabTitle =
      l10n_util::GetNSStringWithFixup(IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB);
  ProceduralBlock openInNewTabAction = ^{
    [weakSelf openURLInNewTab:entry.URL];
    [weakSelf dismissContextMenuCoordinator];
  };
  [self.contextMenuCoordinator addItemWithTitle:openInNewTabTitle
                                         action:openInNewTabAction
                                          style:UIAlertActionStyleDefault];

  if (base::ios::IsMultipleScenesSupported()) {
    // Add "Open In New Window" option.
    NSString* openInNewWindowTitle =
        l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_OPENINNEWWINDOW);
    ProceduralBlock openInNewWindowAction = ^{
      [weakSelf openURLInNewWindow:entry.URL];
    };
    [self.contextMenuCoordinator addItemWithTitle:openInNewWindowTitle
                                           action:openInNewWindowAction
                                            style:UIAlertActionStyleDefault];
  }

  // Add "Open in New Incognito Tab" option.
  NSString* openInNewIncognitoTabTitle = l10n_util::GetNSStringWithFixup(
      IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWINCOGNITOTAB);
  ProceduralBlock openInNewIncognitoTabAction = ^{
    [weakSelf openURLInNewIncognitoTab:entry.URL];
    [weakSelf dismissContextMenuCoordinator];
  };
  BOOL incognitoEnabled =
      !IsIncognitoModeDisabled(self.browser->GetBrowserState()->GetPrefs());
  [self.contextMenuCoordinator addItemWithTitle:openInNewIncognitoTabTitle
                                         action:openInNewIncognitoTabAction
                                          style:UIAlertActionStyleDefault
                                        enabled:incognitoEnabled];

  // Add "Copy URL" option.
  NSString* copyURLTitle =
      l10n_util::GetNSStringWithFixup(IDS_IOS_CONTENT_CONTEXT_COPY);
  ProceduralBlock copyURLAction = ^{
    StoreURLInPasteboard(entry.URL);
    [weakSelf dismissContextMenuCoordinator];
  };
  [self.contextMenuCoordinator addItemWithTitle:copyURLTitle
                                         action:copyURLAction
                                          style:UIAlertActionStyleDefault];

  [self.contextMenuCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_APP_CANCEL)
                action:^{
                  [weakSelf dismissContextMenuCoordinator];
                }
                 style:UIAlertActionStyleCancel];
  [self.contextMenuCoordinator start];
}

// Opens URL in a new non-incognito tab and dismisses the history view.
- (void)openURLInNewTab:(const GURL&)URL {
  base::RecordAction(
      base::UserMetricsAction("MobileHistoryPage_EntryLinkOpenNewTab"));
  UrlLoadParams params = UrlLoadParams::InNewTab(URL);
  __weak __typeof(self) weakSelf = self;
  [self.delegate
      dismissHistoryTableViewController:self
                         withCompletion:^{
                           [weakSelf
                               loadAndActivateTabFromHistoryWithParams:params
                                                             incognito:NO];
                         }];
}

// Opens URL in a new non-incognito tab in a new window and dismisses the
// history view.
- (void)openURLInNewWindow:(const GURL&)URL {
  if (!self.browser) {
    return;
  }
  id<ApplicationCommands> windowOpener = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  [windowOpener
      openNewWindowWithActivity:ActivityToLoadURL(WindowActivityHistoryOrigin,
                                                  URL)];
}

// Opens URL in a new incognito tab and dismisses the history view.
- (void)openURLInNewIncognitoTab:(const GURL&)URL {
  base::RecordAction(base::UserMetricsAction(
      "MobileHistoryPage_EntryLinkOpenNewIncognitoTab"));
  UrlLoadParams params = UrlLoadParams::InNewTab(URL);
  params.in_incognito = YES;
  __weak __typeof(self) weakSelf = self;
  [self.delegate
      dismissHistoryTableViewController:self
                         withCompletion:^{
                           [weakSelf
                               loadAndActivateTabFromHistoryWithParams:params
                                                             incognito:YES];
                         }];
}

#pragma mark Helper Methods

// Loads and opens a tab using `params`. If `incognito` is YES the tab will be
// opened in incognito mode.
- (void)loadAndActivateTabFromHistoryWithParams:(const UrlLoadParams&)params
                                      incognito:(BOOL)incognito {
  if (!self.browser)
    return;

  UrlLoadingBrowserAgent::FromBrowser(_browser)->Load(params);
  if (incognito) {
    [self.presentationDelegate showActiveIncognitoTabFromHistory];
  } else {
    [self.presentationDelegate showActiveRegularTabFromHistory];
  }
}

// Returns YES if the history is actually empty, and the user is neither
// searching nor editing.
- (BOOL)isEmptyState {
  return !self.loading && self.empty && !self.searchInProgress;
}

- (UIBarButtonItem*)createSpacerButton {
  return [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace
                           target:nil
                           action:nil];
}

// Returns the toolbar buttons for the current state.
- (NSArray<UIBarButtonItem*>*)toolbarButtons {
  if ([self isEmptyState]) {
    return @[
      [self createSpacerButton], self.clearBrowsingDataButton,
      [self createSpacerButton]
    ];
  }
  if (self.isEditing) {
    return @[ self.deleteButton, [self createSpacerButton], self.cancelButton ];
  }
  return @[
    self.clearBrowsingDataButton, [self createSpacerButton], self.editButton
  ];
}

// Adds a view as background of the TableView.
- (void)addEmptyTableViewBackground {
  [self addEmptyTableViewWithImage:[UIImage imageNamed:@"history_empty"]
                             title:l10n_util::GetNSString(
                                       IDS_IOS_HISTORY_EMPTY_TITLE)
                          subtitle:l10n_util::GetNSString(
                                       IDS_IOS_HISTORY_EMPTY_MESSAGE)];
  [self updateNavigationBar];
}

// Clears the background of the TableView.
- (void)removeEmptyTableViewBackground {
  [self removeEmptyTableView];
  [self updateNavigationBar];
}

// Opens URL in the current tab and dismisses the history view.
- (void)openURL:(const GURL&)URL {
  if (!self.browser) {
    return;
  }
  new_tab_page_uma::RecordAction(
      self.browser->GetBrowserState()->IsOffTheRecord(),
      self.browser->GetWebStateList()->GetActiveWebState(),
      new_tab_page_uma::ACTION_OPENED_HISTORY_ENTRY);
  UrlLoadParams params = UrlLoadParams::InCurrentTab(URL);
  params.web_params.transition_type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
  params.load_strategy = self.loadStrategy;
  __weak __typeof(self) weakSelf = self;
  [self.delegate
      dismissHistoryTableViewController:self
                         withCompletion:^{
                           [weakSelf
                               loadAndActivateTabFromHistoryWithParams:params
                                                             incognito:NO];
                         }];
}

// Dismisses this ViewController.
- (void)dismissHistory {
  base::RecordAction(base::UserMetricsAction("MobileHistoryClose"));
  [self.delegate dismissHistoryTableViewController:self withCompletion:nil];
}

- (void)openPrivacySettings {
  base::RecordAction(
      base::UserMetricsAction("HistoryPage_InitClearBrowsingData"));
  [self.delegate displayClearHistoryData];
}

#pragma mark - Accessibility

- (BOOL)accessibilityPerformEscape {
  [self.delegate dismissHistoryTableViewController:self withCompletion:nil];
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
