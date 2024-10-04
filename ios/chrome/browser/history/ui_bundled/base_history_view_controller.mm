// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/history/ui_bundled/base_history_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/i18n/time_formatting.h"
#import "base/ios/ios_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/browsing_data/core/browsing_data_utils.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/base/data_type.h"
#import "components/sync/service/sync_service.h"
#import "components/url_formatter/elide_url.h"
#import "components/url_formatter/url_formatter.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/drag_and_drop/model/drag_item_util.h"
#import "ios/chrome/browser/drag_and_drop/model/table_view_url_drag_drop_handler.h"
#import "ios/chrome/browser/history/ui_bundled/base_history_view_controller+subclassing.h"
#import "ios/chrome/browser/history/ui_bundled/history_entries_status_item.h"
#import "ios/chrome/browser/history/ui_bundled/history_entries_status_item_delegate.h"
#import "ios/chrome/browser/history/ui_bundled/history_entry_inserter.h"
#import "ios/chrome/browser/history/ui_bundled/history_entry_item.h"
#import "ios/chrome/browser/history/ui_bundled/history_menu_provider.h"
#import "ios/chrome/browser/history/ui_bundled/history_table_view_controller_delegate.h"
#import "ios/chrome/browser/history/ui_bundled/history_ui_constants.h"
#import "ios/chrome/browser/history/ui_bundled/history_util.h"
#import "ios/chrome/browser/history/ui_bundled/public/history_presentation_delegate.h"
#import "ios/chrome/browser/keyboard/ui_bundled/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/metrics/model/new_tab_page_uma.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/quick_delete_commands.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_link_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_favicon_data_source.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller_constants.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/pasteboard_util.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/features.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/browser/window_activities/model/window_activity_helpers.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/referrer.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/strings/grit/ui_strings.h"

using history::BrowsingHistoryService;

namespace {
enum ItemType : NSInteger {
  kItemTypeHistoryEntry = kItemTypeEnumZero,
  kItemTypeEntriesStatus,
  kItemTypeEntriesStatusWithLink,
  kItemTypeActivityIndicator,
};
// Section identifier for the header (sync information) section.
const NSInteger kEntriesStatusSectionIdentifier = kSectionIdentifierEnumZero;
// Maximum number of entries to retrieve in a single query to history service.
const int kMaxFetchCount = 100;
// Separation space between sections.
const CGFloat kSeparationSpaceBetweenSections = 9;
}  // namespace

@interface BaseHistoryViewController () <
    HistoryEntriesStatusItemDelegate,
    HistoryEntryInserterDelegate,
    TableViewLinkHeaderFooterItemDelegate> {
  // Closure to request next page of history.
  base::OnceClosure _query_history_continuation;
  // The current status message for the tableView, it might be nil.
  NSString* _currentStatusMessage;
  // The current query for visible history entries.
  NSString* _currentQuery;
  // YES if there are no more history entries to load.
  BOOL _finishedLoading;
  // Handler for URL drag interactions.
  TableViewURLDragDropHandler* _dragDropHandler;
}
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
@end

@implementation BaseHistoryViewController

#pragma mark - Public

- (instancetype)init {
  UITableViewStyle style = ChromeTableViewStyle();
  return [super initWithStyle:style];
}

- (void)detachFromBrowser {
  // Clear C++ ivars.
  _browser = nullptr;
  _historyService = nullptr;
  [self dismissContextMenuCoordinator];
}

#pragma mark - ViewController Lifecycle.

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

  _dragDropHandler = [[TableViewURLDragDropHandler alloc] init];
  _dragDropHandler.origin = WindowActivityHistoryOrigin;
  _dragDropHandler.dragDataSource = self;
  self.tableView.dragDelegate = _dragDropHandler;
  self.tableView.dragInteractionEnabled = true;
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
}

#pragma mark - Subclassing properties

- (void)showHistoryMatchingQuery:(NSString*)query {
  _finishedLoading = NO;
  _currentQuery = query;
  [self fetchHistoryForQuery:query continuation:false];
}

- (void)filterResults:(NSMutableArray*)resultsItems
          searchQuery:(NSString*)searchQuery {
  if (([searchQuery length] > 0 &&
       [_currentQuery isEqualToString:searchQuery]) ||
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
}

- (BOOL)checkEmptyHistory:
    (const std::vector<BrowsingHistoryService::HistoryEntry>&)results {
  return results.empty() && self.empty;
}

- (BOOL)shouldDisplayLoadingIndicator {
  return self.empty;
}

- (void)fetchHistoryForQuery:(NSString*)query continuation:(BOOL)continuation {
  if (!self.browser) {
    return;
  }

  if (!self.historyService) {
    return;
  }

  self.loading = YES;
  if ([self shouldDisplayLoadingIndicator]) {
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

- (void)updateTableViewAfterDeletingEntries {
  // If only the header section remains, there are no history entries.
  if ([self.tableViewModel numberOfSections] == 1) {
    self.empty = YES;
    [self addEmptyTableViewBackground];
  }
  [self updateEntriesStatusMessageWithMessage:nil messageWillContainLink:NO];
}

- (void)addEmptyTableViewBackground {
  [self addEmptyTableViewWithImage:[UIImage imageNamed:@"history_empty"]
                             title:l10n_util::GetNSString(
                                       IDS_IOS_HISTORY_EMPTY_TITLE)
                          subtitle:l10n_util::GetNSString(
                                       IDS_IOS_HISTORY_EMPTY_MESSAGE)];
}

- (void)removeEmptyTableViewBackground {
  [self removeEmptyTableView];
}

// TODO(crbug.com/369517575): Clean-up deprecated implementation of the Context
// menu.
#pragma mark - Context Menu

// Displays a context menu on the cell pressed with gestureRecognizer.
- (void)displayContextMenuInvokedByGestureRecognizer:
    (UILongPressGestureRecognizer*)gestureRecognizer {
  if (!self.browser) {
    return;
  }
  if (gestureRecognizer.numberOfTouches != 1 ||
      gestureRecognizer.state != UIGestureRecognizerStateBegan) {
    return;
  }

  CGPoint touchLocation = [gestureRecognizer locationOfTouch:0
                                                      inView:self.tableView];
  NSIndexPath* touchedItemIndexPath =
      [self.tableView indexPathForRowAtPoint:touchLocation];
  // If there's no index path, or the index path is for the header item, do not
  // display a contextual menu.
  if (!touchedItemIndexPath ||
      [touchedItemIndexPath isEqual:[NSIndexPath indexPathForItem:0
                                                        inSection:0]]) {
    return;
  }

  HistoryEntryItem* entry = base::apple::ObjCCastStrict<HistoryEntryItem>(
      [self.tableViewModel itemAtIndexPath:touchedItemIndexPath]);

  __weak BaseHistoryViewController* weakSelf = self;
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
      !IsIncognitoModeDisabled(self.browser->GetProfile()->GetPrefs());
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
  [self.delegate dismissViewController:self
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
  [self.delegate dismissViewController:self
                        withCompletion:^{
                          [weakSelf
                              loadAndActivateTabFromHistoryWithParams:params
                                                            incognito:YES];
                        }];
}

#pragma mark - HistoryConsumer

// Tells the consumer that the result of a history query has been retrieved.
// Entries in `result` are already sorted.
- (void)historyQueryWasCompletedWithResults:
            (const std::vector<BrowsingHistoryService::HistoryEntry>&)results
                           queryResultsInfo:
                               (const BrowsingHistoryService::QueryResultsInfo&)
                                   queryResultsInfo
                        continuationClosure:
                            (base::OnceClosure)continuationClosure {
  if (!self.browser) {
    return;
  }

  self.loading = NO;
  _query_history_continuation = std::move(continuationClosure);

  // If history sync is enabled and there hasn't been a response from synced
  // history, try fetching again.
  syncer::SyncService* syncService =
      SyncServiceFactory::GetForProfile(self.browser->GetProfile());
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
  if ([self checkEmptyHistory:results]) {
    [self addEmptyTableViewBackground];
    return;
  }

  _finishedLoading = queryResultsInfo.reached_beginning;
  self.empty = NO;
  [self removeEmptyTableViewBackground];

  // Header section should be updated outside of batch updates, otherwise
  // loading indicator removal will not be observed.
  [self updateEntriesStatusMessageWithMessage:nil messageWillContainLink:NO];

  NSMutableArray* resultsItems = [NSMutableArray array];
  NSString* searchQuery =
      [base::SysUTF16ToNSString(queryResultsInfo.search_text) copy];

  // There should always be at least a header section present.
  DCHECK([[self tableViewModel] numberOfSections]);
  for (const BrowsingHistoryService::HistoryEntry& entry : results) {
    HistoryEntryItem* item =
        [[HistoryEntryItem alloc] initWithType:kItemTypeHistoryEntry
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

  // There could be child classes that need to filter the results. Child classes
  // are responsible for implementing this method based on their needs.
  [self filterResults:resultsItems searchQuery:searchQuery];

  // Insert result items into the model.
  for (HistoryEntryItem* item in resultsItems) {
    [_entryInserter insertHistoryEntryItem:item];
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
    [self updateEntriesStatusMessageWithMessage:nil messageWillContainLink:NO];
  }
}

- (void)historyWasDeleted {
  // If history has been deleted, reload history filtering for the current
  // results. This only observes local changes to history, i.e. removing
  // history via delete browsing data.
  self.filterQueryResult = YES;
  [self showHistoryMatchingQuery:nil];
}

#pragma mark - HistoryEntriesStatusItemDelegate

- (void)historyEntriesStatusItem:(HistoryEntriesStatusItem*)item
               didRequestOpenURL:(const GURL&)URL {
  // TODO(crbug.com/41366648): Migrate. This will navigate to the status message
  // "Show Full History" URL.
}

#pragma mark - HistoryEntryInserterDelegate

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

#pragma mark - HistoryEntryItemDelegate

- (void)historyEntryItemDidRequestOpen:(HistoryEntryItem*)item {
  [self openURL:item.URL];
}

- (void)historyEntryItemDidRequestDelete:(HistoryEntryItem*)item {
  NSInteger sectionIdentifier =
      [_entryInserter sectionIdentifierForTimestamp:item.timestamp];
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
      [_entryInserter sectionIdentifierForTimestamp:item.timestamp];
  // If the item is still in the model, reconfigure it.
  if ([self.tableViewModel hasSectionForSectionIdentifier:sectionIdentifier] &&
      [self.tableViewModel hasItem:item
           inSectionWithIdentifier:sectionIdentifier]) {
    [self reconfigureCellsForItems:@[ item ]];
  }
}

#pragma mark - TableViewLinkHeaderFooterItemDelegate

- (void)view:(TableViewLinkHeaderFooterView*)view didTapLinkURL:(CrURL*)URL {
  [self openURLInNewTab:URL.gurl];
}

#pragma mark - History Data Updates

// Deletes selected items from browser history and removes them from the
// tableView.
- (void)deleteSelectedItemsFromHistory {
  browsing_data::RecordDeleteBrowsingDataAction(
      browsing_data::DeleteBrowsingDataAction::kHistoryPageEntries);

  if (!self.browser) {
    return;
  }

  if (!self.historyService) {
    return;
  }

  // Validate indexes of items to delete and abort if any have been made invalid
  // by a crossing actions (like query refresh or animations).
  NSArray* toDeleteIndexPaths = self.tableView.indexPathsForSelectedRows;
  for (NSIndexPath* indexPath in toDeleteIndexPaths) {
    if (![self.tableViewModel hasItemAtIndexPath:indexPath]) {
      return;
    }
  }

  // Delete items from Browser History.
  std::vector<BrowsingHistoryService::HistoryEntry> entries;
  for (NSIndexPath* indexPath in toDeleteIndexPaths) {
    HistoryEntryItem* object = base::apple::ObjCCastStrict<HistoryEntryItem>(
        [self.tableViewModel itemAtIndexPath:indexPath]);
    BrowsingHistoryService::HistoryEntry entry;
    entry.url = object.URL;
    // TODO(crbug.com/40479288) Remove base::TimeXXX::ToInternalValue().
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
      }];
  base::RecordAction(base::UserMetricsAction("HistoryPage_RemoveSelected"));
}

#pragma mark - UITableViewDelegate

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  if ([self.tableViewModel sectionIdentifierForSectionIndex:section] ==
      kEntriesStatusSectionIdentifier) {
    return 0;
  }
  return kSeparationSpaceBetweenSections;
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  // Hide the status header if the currentStatusMessage is nil.
  if ([self.tableViewModel sectionIdentifierForSectionIndex:section] ==
          kEntriesStatusSectionIdentifier &&
      [_currentStatusMessage length] == 0) {
    return 0;
  }
  return UITableViewAutomaticDimension;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ(tableView, self.tableView);
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  // Only navigate and record metrics if a kItemTypeHistoryEntry was selected.
  if (item.type == kItemTypeHistoryEntry) {
    HistoryEntryItem* historyItem =
        base::apple::ObjCCastStrict<HistoryEntryItem>(item);
    [self openURL:historyItem.URL];
  }
}

- (BOOL)tableView:(UITableView*)tableView
    canEditRowAtIndexPath:(NSIndexPath*)indexPath {
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  return (item.type == kItemTypeHistoryEntry);
}

- (UIContextMenuConfiguration*)tableView:(UITableView*)tableView
    contextMenuConfigurationForRowAtIndexPath:(NSIndexPath*)indexPath
                                        point:(CGPoint)point {
  if (![self.tableViewModel hasItemAtIndexPath:indexPath]) {
    // It's possible that indexPath is invalid due to crossing action (like
    // query refresh or animations).
    return nil;
  }
  if (indexPath.section ==
      [self.tableViewModel
          sectionForSectionIdentifier:kEntriesStatusSectionIdentifier]) {
    return nil;
  }

  HistoryEntryItem* entry = base::apple::ObjCCastStrict<HistoryEntryItem>(
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
          base::apple::ObjCCast<TableViewLinkHeaderFooterView>(view);
      linkView.delegate = self;
    } break;
    default:
      break;
  }
  return view;
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cellToReturn = [super tableView:tableView
                             cellForRowAtIndexPath:indexPath];
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  cellToReturn.userInteractionEnabled = !(item.type == kItemTypeEntriesStatus);
  if (item.type == kItemTypeHistoryEntry) {
    HistoryEntryItem* URLItem =
        base::apple::ObjCCastStrict<HistoryEntryItem>(item);
    TableViewURLCell* URLCell =
        base::apple::ObjCCastStrict<TableViewURLCell>(cellToReturn);
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
  if (_finishedLoading) {
    return;
  }

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

#pragma mark - TableViewURLDragDataSource

// Returns a wrapper object with URL and title to drag for the item at
// `indexPath` in `tableView`. Returns nil if item at `indexPath` is not
// draggable.
- (URLInfo*)tableView:(UITableView*)tableView
    URLInfoAtIndexPath:(NSIndexPath*)indexPath {
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  switch (item.type) {
    case kItemTypeHistoryEntry: {
      HistoryEntryItem* URLItem =
          base::apple::ObjCCastStrict<HistoryEntryItem>(item);
      return [[URLInfo alloc] initWithURL:URLItem.URL title:URLItem.text];
    }
    case kItemTypeEntriesStatus:
    case kItemTypeActivityIndicator:
    case kItemTypeEntriesStatusWithLink:
      break;
  }
  return nil;
}

#pragma mark - Private methods

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
  if (deleteItemsFromTableView) {
    [self.tableView deleteRowsAtIndexPaths:indexArray
                          withRowAnimation:UITableViewRowAnimationNone];
  }

  // Remove any empty sections, except the header section.
  for (int section = self.tableView.numberOfSections - 1; section > 0;
       --section) {
    if (![self.tableViewModel numberOfItemsInSection:section]) {
      [_entryInserter removeSection:section];
      if (deleteItemsFromTableView) {
        [self.tableView deleteSections:[NSIndexSet indexSetWithIndex:section]
                      withRowAnimation:UITableViewRowAnimationAutomatic];
      }
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
            base::apple::ObjCCastStrict<HistoryEntryItem>(item);
        if (![entries containsObject:historyItem]) {
          NSIndexPath* indexPath =
              [self.tableViewModel indexPathForItem:historyItem];
          [self.filteredOutEntriesIndexPaths addObject:indexPath];
        }
      }
    }
  }
}

- (void)dismissContextMenuCoordinator {
  [self.contextMenuCoordinator stop];
  self.contextMenuCoordinator = nil;
}

// Updates header section to provide relevant information about the currently
// displayed history entries. There should only ever be at most one item in this
// section.
- (void)updateEntriesStatusMessageWithMessage:(NSString*)message
                       messageWillContainLink:(BOOL)messageWillContainLink {
  // If the new message is the same as the old one, there's no need to do
  // anything else. Compare the objects since they might both be nil.
  if ([_currentStatusMessage isEqualToString:message] ||
      message == _currentStatusMessage) {
    return;
  }

  _currentStatusMessage = message;

  TableViewHeaderFooterItem* item = nil;
  if (messageWillContainLink) {
    TableViewLinkHeaderFooterItem* header =
        [[TableViewLinkHeaderFooterItem alloc]
            initWithType:kItemTypeEntriesStatusWithLink];
    header.text = message;
    header.urls = @[ [[CrURL alloc] initWithGURL:GURL(kHistoryMyActivityURL)] ];
    item = header;
  } else {
    TableViewTextHeaderFooterItem* header =
        [[TableViewTextHeaderFooterItem alloc]
            initWithType:kItemTypeEntriesStatus];
    header.text = message;
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

#pragma mark - Helper Methods

// Loads and opens a tab using `params`. If `incognito` is YES the tab will be
// opened in incognito mode.
- (void)loadAndActivateTabFromHistoryWithParams:(const UrlLoadParams&)params
                                      incognito:(BOOL)incognito {
  if (!self.browser) {
    return;
  }

  UrlLoadingBrowserAgent::FromBrowser(_browser)->Load(params);
  if (incognito) {
    [self.presentationDelegate showActiveIncognitoTabFromHistory];
  } else {
    [self.presentationDelegate showActiveRegularTabFromHistory];
  }
}

// Opens URL in the current tab and dismisses the history view.
- (void)openURL:(const GURL&)URL {
  if (!self.browser) {
    return;
  }
  web::WebState* currentWebState =
      self.browser->GetWebStateList()->GetActiveWebState();
  bool is_ntp =
      currentWebState && currentWebState->GetVisibleURL() == kChromeUINewTabURL;
  new_tab_page_uma::RecordNTPAction(
      self.browser->GetProfile()->IsOffTheRecord(), is_ntp,
      new_tab_page_uma::ACTION_OPENED_HISTORY_ENTRY);
  UrlLoadParams params = UrlLoadParams::InCurrentTab(URL);
  params.web_params.transition_type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
  params.load_strategy = self.loadStrategy;
  __weak __typeof(self) weakSelf = self;
  [self.delegate dismissViewController:self
                        withCompletion:^{
                          [weakSelf
                              loadAndActivateTabFromHistoryWithParams:params
                                                            incognito:NO];
                        }];
}

#pragma mark - Accessibility

- (BOOL)accessibilityPerformEscape {
  [self.delegate dismissViewController:self];
  return YES;
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
  [self.delegate dismissViewController:self];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerWillDismiss:
    (UIPresentationController*)presentationController {
  // Method should be implemented by the child classes.
}

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  base::RecordAction(base::UserMetricsAction("IOSHistoryCloseWithSwipe"));
  // Call the delegate dismissViewController: to
  // clean up state and stop the Coordinator.
  [self.delegate dismissViewController:self];
}

- (NSMutableArray<NSIndexPath*>*)filteredOutEntriesIndexPaths {
  if (!_filteredOutEntriesIndexPaths) {
    _filteredOutEntriesIndexPaths = [[NSMutableArray alloc] init];
  }
  return _filteredOutEntriesIndexPaths;
}
@end
