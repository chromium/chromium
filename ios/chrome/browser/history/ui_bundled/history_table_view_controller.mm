// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/history/ui_bundled/history_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/i18n/time_formatting.h"
#import "base/ios/ios_util.h"
#import "base/metrics/histogram_functions.h"
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
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/strings/grit/ui_strings.h"

using history::BrowsingHistoryService;

namespace {
enum ItemType : NSInteger {
  kItemTypeHistoryEntry = kItemTypeEnumZero,
};
// The default UIButton font size used by UIKit.
const CGFloat kButtonDefaultFontSize = 15.0;
// Horizontal width representing UIButton's padding.
const CGFloat kButtonHorizontalPadding = 30.0;
}  // namespace

@interface HistoryTableViewController () <UISearchControllerDelegate,
                                          UISearchResultsUpdating,
                                          UISearchBarDelegate>

// YES if there is a search happening.
@property(nonatomic, assign) BOOL searchInProgress;
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

#pragma mark - TableViewModel

- (void)viewDidLoad {
  [super viewDidLoad];

  [self showHistoryMatchingQuery:nil];

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

#pragma mark - Superclass overrides

- (void)updateEntriesStatusMessageWithMessage:(NSString*)message
                       messageWillContainLink:(BOOL)messageWillContainLink {
  if (self.empty) {
    message = self.searchController.isActive
                  ? l10n_util::GetNSString(IDS_HISTORY_NO_SEARCH_RESULTS)
                  : nil;
  } else if (self.shouldShowNoticeAboutOtherFormsOfBrowsingHistory &&
             !self.searchController.isActive) {
    message = l10n_util::GetNSString(IDS_IOS_HISTORY_OTHER_FORMS_OF_HISTORY);
    messageWillContainLink = YES;
  }

  [super updateEntriesStatusMessageWithMessage:message
                        messageWillContainLink:messageWillContainLink];
}

- (BOOL)checkEmptyHistory:
    (const std::vector<BrowsingHistoryService::HistoryEntry>&)results {
  return [super checkEmptyHistory:results] && !self.searchInProgress;
}

- (void)handleEmptyHistory {
  [self addEmptyTableViewBackground];
  [self updateToolbarButtonsWithAnimation:NO];
}

- (BOOL)shouldDisplayLoadingIndicator {
  return self.empty && !self.searchInProgress;
}

- (void)updateTableViewAfterDeletingEntries {
  if ([self.tableViewModel numberOfSections] == 1) {
    self.empty = YES;
    if (!self.searchInProgress) {
      [self addEmptyTableViewBackground];
    }
  }
  [self updateEntriesStatusMessageWithMessage:nil messageWillContainLink:NO];
  [self updateToolbarButtonsWithAnimation:YES];

  [self configureViewsForNonEditModeWithAnimation:YES];
}

- (void)addEmptyTableViewBackground {
  [super addEmptyTableViewBackground];
  [self updateNavigationBar];
}

- (void)removeEmptyTableViewBackground {
  [super removeEmptyTableViewBackground];
  [self updateNavigationBar];
}

- (void)filterResults:(NSMutableArray*)resultsItems
          searchQuery:(NSString*)searchQuery {
  if (self.searchInProgress || self.filterQueryResult) {
    [super filterResults:resultsItems searchQuery:searchQuery];
  }
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ(tableView, self.tableView);
  if (self.isEditing) {
    [self updateToolbarButtonsWithAnimation:YES];
  } else {
    TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
    // Only navigate and record metrics if a kItemTypeHistoryEntry was selected.
    if (item.type == kItemTypeHistoryEntry) {
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
      [super tableView:tableView didSelectRowAtIndexPath:indexPath];
    }
  }
}

- (UIContextMenuConfiguration*)tableView:(UITableView*)tableView
    contextMenuConfigurationForRowAtIndexPath:(NSIndexPath*)indexPath
                                        point:(CGPoint)point {
  if (self.isEditing) {
    // Don't show the context menu when currently in editing mode.
    return nil;
  }
  return [super tableView:tableView
      contextMenuConfigurationForRowAtIndexPath:indexPath
                                          point:point];
}

- (void)tableView:(UITableView*)tableView
    didDeselectRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ(tableView, self.tableView);
  if (self.editing) {
    [self updateToolbarButtonsWithAnimation:YES];
  }
}

#pragma mark - TableViewURLDragDataSource

- (URLInfo*)tableView:(UITableView*)tableView
    URLInfoAtIndexPath:(NSIndexPath*)indexPath {
  if (self.tableView.isEditing) {
    return nil;
  }
  return [super tableView:tableView URLInfoAtIndexPath:indexPath];
}

- (void)fetchHistoryForQuery:(NSString*)query continuation:(BOOL)continuation {
  self.loading = YES;

  [super fetchHistoryForQuery:query continuation:continuation];
}

#pragma mark - Navigation Toolbar Configuration

// Animates the view configuration after flipping the current status of `[self
// setEditing]`.
- (void)animateViewsConfigurationForEditingChange {
  if (self.isEditing) {
    [self configureViewsForNonEditModeWithAnimation:YES];
  } else {
    [self configureViewsForEditModeWithAnimation:YES];
  }
  // Force update a11y actions based on edit mode.
  for (int section = 0; section < self.tableViewModel.numberOfSections;
       section++) {
    if (![self.tableViewModel numberOfItemsInSection:section]) {
      continue;
    }
    NSInteger sectionIdentifier =
        [self.tableViewModel sectionIdentifierForSectionIndex:section];
    [self reconfigureCellsForItems:
              [self.tableViewModel
                  itemsInSectionWithIdentifier:sectionIdentifier]];
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

#pragma mark - Context Menu

- (void)displayContextMenuInvokedByGestureRecognizer:
    (UILongPressGestureRecognizer*)gestureRecognizer {
  if (self.editing) {
    return;
  }

  if ([self scrimIsVisible]) {
    self.searchController.active = NO;
    return;
  }
  [super displayContextMenuInvokedByGestureRecognizer:gestureRecognizer];
}

#pragma mark - HistoryConsumer

- (void)historyQueryWasCompletedWithResults:
            (const std::vector<BrowsingHistoryService::HistoryEntry>&)results
                           queryResultsInfo:
                               (const BrowsingHistoryService::QueryResultsInfo&)
                                   queryResultsInfo
                        continuationClosure:
                            (base::OnceClosure)continuationClosure {
  [super historyQueryWasCompletedWithResults:results
                            queryResultsInfo:queryResultsInfo
                         continuationClosure:std::move(continuationClosure)];

  if ([self checkEmptyHistory:results]) {
    [self updateToolbarButtonsWithAnimation:NO];
  } else {
    // Update toolbar buttons.
    [self updateToolbarButtonsWithAnimation:YES];
  }
}

#pragma mark - UISearchResultsUpdating

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

#pragma mark - UISearchControllerDelegate

- (void)willPresentSearchController:(UISearchController*)searchController {
  [self showScrim];
}

- (void)didDismissSearchController:(UISearchController*)searchController {
  [self hideScrim];
}

#pragma mark - UISearchBarDelegate

- (void)searchBarTextDidBeginEditing:(UISearchBar*)searchBar {
  self.searchInProgress = YES;
  [self updateEntriesStatusMessageWithMessage:nil messageWillContainLink:NO];
}

- (void)searchBarTextDidEndEditing:(UISearchBar*)searchBar {
  self.searchInProgress = NO;
  [self updateEntriesStatusMessageWithMessage:nil messageWillContainLink:NO];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerWillDismiss:
    (UIPresentationController*)presentationController {
  if (self.searchInProgress) {
    // Dismiss the keyboard if trying to dismiss the VC so the keyboard doesn't
    // linger until the VC dismissal has completed.
    [self.searchController.searchBar endEditing:YES];
  }
}

#pragma mark - Private methods

// Dismisses this ViewController.
- (void)dismissHistory {
  base::RecordAction(base::UserMetricsAction("MobileHistoryClose"));
  [self.delegate dismissViewController:self];
}

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

- (UIBarButtonItem*)createSpacerButton {
  return [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace
                           target:nil
                           action:nil];
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

#pragma mark - Helper Methods

// Returns YES if the history is actually empty, and the user is neither
// searching nor editing.
- (BOOL)isEmptyState {
  return !self.loading && self.empty && !self.searchInProgress;
}

- (void)openPrivacySettings {
  base::RecordAction(
      base::UserMetricsAction("HistoryPage_InitClearBrowsingData"));
  base::UmaHistogramEnumeration(
      browsing_data::kDeleteBrowsingDataDialogHistogram,
      browsing_data::DeleteBrowsingDataDialogAction::
          kHistoryEntryPointSelected);

  if (IsIosQuickDeleteEnabled()) {
    if (!self.browser) {
      return;
    }
    id<QuickDeleteCommands> quickDeleteHandler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), QuickDeleteCommands);
    [quickDeleteHandler
        showQuickDeleteAndCanPerformTabsClosureAnimation:
            ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET];
    return;
  }

  [self.delegate displayClearHistoryData];
}

#pragma mark - Setter & Getters

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

// TODO(crbug.com/41382611): Find a way to disable the button when a VC is
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

@end
