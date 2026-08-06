// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_search_view_controller.h"

#import "base/notreached.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_commands.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Section identifiers in the "AtMemory" page table view.
enum class SectionIdentifier {
  kSearchSection,
  kSearchFooterSection,
  kFetchingSection,
  kNoDataSection,
  kNoConnectionSection,
  kUnsupportedQuerySection,
  kNoticeSection,
  kRecentFillsSection,
  kSearchResultsSection,
};

// Item identifiers in the "AtMemory" page table view.
enum class ItemIdentifier {
  kSearchItem,
  kFetchingItem,
  kNoDataItem,
  kNoConnectionItem,
  kUnsupportedQueryItem,
  kNoticeItem,
};

// View states for the AtMemory search table view.
enum class ViewState {
  kInitialState,
  kSearchState,
  kFetchingState,
  kErrorState,
  kResultState,
};

}  // namespace

@interface AtMemorySearchViewController () <UISearchResultsUpdating>
@end

@implementation AtMemorySearchViewController {
  // The table view for this view controller.
  UITableViewDiffableDataSource<NSNumber*, NSNumber*>* _dataSource;
  // Search controller for users to type a query for performing an AtMemory
  // search and filtering items.
  UISearchController* _searchController;

  // Tells if the notice is visible.
  BOOL _noticeIsVisible;
  // Tells if the recent fills are visible.
  BOOL _recentFillsAreVisible;
  // Represent the error type.
  AtMemoryErrorType _errorType;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  _searchController =
      [[UISearchController alloc] initWithSearchResultsController:nil];
  _searchController.obscuresBackgroundDuringPresentation = NO;
  _searchController.hidesNavigationBarDuringPresentation = NO;
  _searchController.searchResultsUpdater = self;
  _searchController.searchBar.accessibilityIdentifier =
      kAtMemorySearchBarAccessibilityIdentifier;

  self.definesPresentationContext = YES;
  self.navigationItem.searchController = _searchController;
  self.navigationItem.hidesSearchBarWhenScrolling = NO;

  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(handleCancelButton)];
  cancelButton.accessibilityIdentifier =
      kAtMemoryCloseButtonAccessibilityIdentifier;
  self.navigationItem.rightBarButtonItem = cancelButton;

  self.title =
      (self.traitCollection.horizontalSizeClass ==
           UIUserInterfaceSizeClassCompact ||
       self.traitCollection.verticalSizeClass ==
           UIUserInterfaceSizeClassCompact)
          ? l10n_util::GetNSString(IDS_IOS_AUTOFILL_AI_FIND_AND_FILL_TITLE)
          : l10n_util::GetNSString(
                IDS_IOS_PERSONAL_CONTEXT_AUTOFILL_SETTINGS_SWITCH_TITLE);

  [self loadModel];
}

- (void)loadModel {
  __weak __typeof(self) weakSelf = self;
  _dataSource = [[UITableViewDiffableDataSource alloc]
      initWithTableView:self.tableView
           cellProvider:^UITableViewCell*(UITableView* tableView,
                                          NSIndexPath* indexPath,
                                          NSNumber* itemIdentifier) {
             return
                 [weakSelf cellForTableView:tableView
                                  indexPath:indexPath
                             itemIdentifier:static_cast<ItemIdentifier>(
                                                itemIdentifier.integerValue)];
           }];
  NSDiffableDataSourceSnapshot* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];
  [self populateAndApplySnapshot:snapshot
                    forViewState:ViewState::kInitialState];
}

#pragma mark - UISearchResultsUpdating

- (void)updateSearchResultsForSearchController:
    (UISearchController*)searchController {
  // TODO(crbug.com/522338028): Update search cell
}

#pragma mark - Actions

- (void)handleCancelButton {
  [self.atMemoryHandler dismissAtMemory];
}

#pragma mark - AtMemorySearchConsumer

- (void)setErrorType:(AtMemoryErrorType)errorType {
  _errorType = errorType;
}

- (void)setNoticeVisible:(BOOL)noticeVisible {
  _noticeIsVisible = noticeVisible;
}

- (void)setFetchingSubtitle {
  // TODO(crbug.com/541237598): Implement fetching subtitle.
}

- (void)setRecentFills {
  // TODO(crbug.com/540877897): Implement recent fills.
}

- (void)updateTableViewBackgroundStyle:(AtMemoryBackgroundStyle)style {
  switch (style) {
    case AtMemoryBackgroundStyle::kEmptyStyle:
      [self setEmptyTableViewBackground];
      break;
    case AtMemoryBackgroundStyle::kDefaultStyle:
      self.tableView.backgroundView = nil;
      break;
  }
}

#pragma mark - Private

// Applies the diffable data source snapshot for the given `viewState`.
- (void)populateAndApplySnapshot:(NSDiffableDataSourceSnapshot*)snapshot
                    forViewState:(ViewState)viewState {
  switch (viewState) {
    case ViewState::kInitialState:
      [self populateSnapshotForInitialState:snapshot];
      break;
    case ViewState::kSearchState:
    case ViewState::kFetchingState:
    case ViewState::kErrorState:
    case ViewState::kResultState:
      self.tableView.backgroundView = nil;
      break;
  }

  [_dataSource applySnapshot:snapshot animatingDifferences:YES];
}

// Populates `snapshot` for the initial state.
- (void)populateSnapshotForInitialState:
    (NSDiffableDataSourceSnapshot*)snapshot {
  if (_noticeIsVisible) {
    [snapshot appendSectionsWithIdentifiers:@[
      @(static_cast<int>(SectionIdentifier::kNoticeSection))
    ]];
    [snapshot
        appendItemsWithIdentifiers:@[ @(static_cast<int>(
                                       ItemIdentifier::kNoticeItem)) ]
         intoSectionWithIdentifier:@(static_cast<int>(
                                       SectionIdentifier::kNoticeSection))];
  }

  if (_recentFillsAreVisible) {
    [snapshot appendSectionsWithIdentifiers:@[
      @(static_cast<int>(SectionIdentifier::kRecentFillsSection))
    ]];
    // TODO(crbug.com/540877897): Call a method that adds the recentFill
    // results into the kRecentFillsSection.
  }
}

// Sets the table view background to the empty state.
- (void)setEmptyTableViewBackground {
  UIImage* image = [UIImage imageNamed:@"at_memory_empty"];
  [self addEmptyTableViewWithMessage:
            l10n_util::GetNSString(IDS_AUTOFILL_AT_MEMORY_ZERO_STATE_SUBTITLE)
                               image:image];
}

// Reloads the snapshot for the cell with the given `itemIdentifier`.
- (void)updateSnapshotForItemIdentifier:(ItemIdentifier)itemIdentifier {
  NSDiffableDataSourceSnapshot<NSNumber*, NSNumber*>* snapshot =
      [_dataSource snapshot];
  [snapshot
      reloadItemsWithIdentifiers:@[ @(static_cast<int>(itemIdentifier)) ]];
  [_dataSource applySnapshot:snapshot animatingDifferences:YES];
}

// Returns the cell for the corresponding `itemIdentifier`.
- (UITableViewCell*)cellForTableView:(UITableView*)tableView
                           indexPath:(NSIndexPath*)indexPath
                      itemIdentifier:(ItemIdentifier)itemIdentifier {
  switch (itemIdentifier) {
    case ItemIdentifier::kNoticeItem:
    case ItemIdentifier::kSearchItem:
    case ItemIdentifier::kFetchingItem:
    case ItemIdentifier::kNoDataItem:
    case ItemIdentifier::kNoConnectionItem:
    case ItemIdentifier::kUnsupportedQueryItem:
      // TODO(crbug.com/542645452): Implement cells for each item identifier.
      break;
  }
  NOTREACHED();
}

@end
