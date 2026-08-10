// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_search_view_controller.h"

#import "base/check.h"
#import "base/notreached.h"
#import "build/branding_buildflags.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_commands.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_constants.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_inline_notice_view.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_search_consumer.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_search_mutator.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/colorful_symbol_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
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

// The symbol point size for the cell icons.
constexpr CGFloat kIconPointSize = 24;

}  // namespace

@interface AtMemorySearchViewController () <UISearchResultsUpdating,
                                            AtMemoryInlineNoticeViewDelegate>
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
  // The current error type.
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

  self.title = l10n_util::GetNSString(IDS_IOS_AUTOFILL_AI_FIND_AND_FILL_TITLE);

  [self loadModel];
}

- (void)loadModel {
  [AtMemoryInlineNoticeConfiguration registerCellForTableView:self.tableView];
  [TableViewCellContentConfiguration registerCellForTableView:self.tableView];

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
  [self createSnapshotForViewState:AtMemoryViewState::kInitialState];
}

#pragma mark - UISearchResultsUpdating

- (void)updateSearchResultsForSearchController:
    (UISearchController*)searchController {
  [self.mutator startSearchWithQuery:searchController.searchBar.text];
}

#pragma mark - Actions

- (void)handleCancelButton {
  [self.atMemoryHandler dismissAtMemory];
}

#pragma mark - AtMemorySearchConsumer

- (void)setErrorType:(AtMemoryErrorType)errorType {
  _errorType = errorType;
  [self createSnapshotForViewState:AtMemoryViewState::kErrorState];
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

// Creates the diffable data source snapshot for the given `viewState`.
- (void)createSnapshotForViewState:(AtMemoryViewState)viewState {
  switch (viewState) {
    case AtMemoryViewState::kInitialState:
      [self createSnapshotForInitialState];
      return;
    case AtMemoryViewState::kErrorState:
      self.tableView.backgroundView = nil;
      [self createSnapshotForErrorState];
      return;
    case AtMemoryViewState::kSearchState:
    case AtMemoryViewState::kFetchingState:
    case AtMemoryViewState::kResultState:
      self.tableView.backgroundView = nil;
      return;
  }
  NOTREACHED();
}

// Creates the `snapshot` for the initial state.
- (void)createSnapshotForInitialState {
  NSDiffableDataSourceSnapshot* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];

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

  [_dataSource applySnapshot:snapshot animatingDifferences:YES];
}

// Creates the `snapshot` for the error states.
- (void)createSnapshotForErrorState {
  NSDiffableDataSourceSnapshot* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];

  ItemIdentifier errorIdentifier;
  SectionIdentifier errorSection;

  switch (_errorType) {
    case AtMemoryErrorType::kNoConnectionError:
      errorIdentifier = ItemIdentifier::kNoConnectionItem;
      errorSection = SectionIdentifier::kNoConnectionSection;
      break;
    case AtMemoryErrorType::kNoDataError:
      errorIdentifier = ItemIdentifier::kNoDataItem;
      errorSection = SectionIdentifier::kNoDataSection;
      break;
    case AtMemoryErrorType::kUnsupportedQueryError:
      errorIdentifier = ItemIdentifier::kUnsupportedQueryItem;
      errorSection = SectionIdentifier::kUnsupportedQuerySection;
      break;
  }

  // Add the correct error section identifier.
  [snapshot
      appendSectionsWithIdentifiers:@[ @(static_cast<int>(errorSection)) ]];
  [snapshot appendItemsWithIdentifiers:@[ @(static_cast<int>(errorIdentifier)) ]
             intoSectionWithIdentifier:@(static_cast<int>(errorSection))];

  // Add the notice if needed below the error cell.
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

  [_dataSource applySnapshot:snapshot animatingDifferences:YES];
}

// Sets the table view background to the empty state.
- (void)setEmptyTableViewBackground {
  UIImage* image = [UIImage imageNamed:@"at_memory_empty"];
  [self addEmptyTableViewWithMessage:
            l10n_util::GetNSString(IDS_AUTOFILL_AT_MEMORY_ZERO_STATE_SUBTITLE)
                               image:image];
}

// Returns the cell for the corresponding `itemIdentifier`.
- (UITableViewCell*)cellForTableView:(UITableView*)tableView
                           indexPath:(NSIndexPath*)indexPath
                      itemIdentifier:(ItemIdentifier)itemIdentifier {
  UITableViewCell* cell = nil;
  switch (itemIdentifier) {
    case ItemIdentifier::kNoticeItem:
      cell = [self noticeCellForTableView:tableView];
      break;
    case ItemIdentifier::kSearchItem:
    case ItemIdentifier::kFetchingItem:
      // TODO(crbug.com/542645452): Implement cells for each item identifier.
      break;
    case ItemIdentifier::kNoDataItem:
      cell = [self noDataCellForTableView:tableView];
      break;
    case ItemIdentifier::kNoConnectionItem:
      cell = [self noConnectionCellForTableView:tableView];
      break;
    case ItemIdentifier::kUnsupportedQueryItem:
      cell = [self unsupportedQueryCellForTableView:tableView];
      break;
  }
  CHECK(cell);
  return cell;
}

// Returns the table view cell for the "No Data" state.
- (UITableViewCell*)noDataCellForTableView:(UITableView*)tableView {
  TableViewCellContentConfiguration* configuration =
      [[TableViewCellContentConfiguration alloc] init];
  configuration.title = l10n_util::GetNSString(IDS_AUTOFILL_AT_MEMORY_NO_DATA);
  configuration.titleColor = [UIColor colorNamed:kTextPrimaryColor];

  ColorfulSymbolContentConfiguration* symbolConfiguration =
      [[ColorfulSymbolContentConfiguration alloc] init];
  symbolConfiguration.symbolImage =
      DefaultSymbolWithPointSize(kErrorCircleSymbol, kIconPointSize);
  symbolConfiguration.symbolTintColor =
      [UIColor colorNamed:kTextSecondaryColor];
  configuration.leadingConfiguration = symbolConfiguration;

  UITableViewCell* cell =
      [TableViewCellContentConfiguration dequeueTableViewCell:tableView];
  cell.contentConfiguration = configuration;
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  cell.contentView.alpha = kDefaultCellAlpha;
  cell.userInteractionEnabled = NO;
  cell.accessibilityIdentifier = kAtMemoryNoDataCellAccessibilityIdentifier;

  return cell;
}

// Returns the table view cell for the "No Connection" state.
- (UITableViewCell*)noConnectionCellForTableView:(UITableView*)tableView {
  TableViewCellContentConfiguration* configuration =
      [[TableViewCellContentConfiguration alloc] init];
  configuration.title = _searchController.searchBar.text;
  configuration.titleColor = [UIColor colorNamed:kTextPrimaryColor];
  configuration.subtitle =
      l10n_util::GetNSString(IDS_AUTOFILL_AT_MEMORY_NO_CONNECTION);

  ColorfulSymbolContentConfiguration* symbolConfiguration =
      [[ColorfulSymbolContentConfiguration alloc] init];
  symbolConfiguration.symbolImage =
      DefaultSymbolWithPointSize(kErrorCircleSymbol, kIconPointSize);
  symbolConfiguration.symbolTintColor =
      [UIColor colorNamed:kTextSecondaryColor];
  configuration.leadingConfiguration = symbolConfiguration;

  UITableViewCell* cell =
      [TableViewCellContentConfiguration dequeueTableViewCell:tableView];
  cell.contentConfiguration = configuration;
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  cell.contentView.alpha = kDisabledCellAlpha;
  cell.userInteractionEnabled = NO;
  cell.accessibilityIdentifier =
      kAtMemoryNoConnectionCellAccessibilityIdentifier;

  return cell;
}

// Returns the table view cell for the "Unsupported Query" state.
- (UITableViewCell*)unsupportedQueryCellForTableView:(UITableView*)tableView {
  TableViewCellContentConfiguration* configuration =
      [[TableViewCellContentConfiguration alloc] init];
  configuration.title =
      l10n_util::GetNSString(IDS_AUTOFILL_AT_MEMORY_UNSUPPORTED_QUERY_TITLE);
  configuration.subtitle = l10n_util::GetNSString(
      IDS_AUTOFILL_AT_MEMORY_UNSUPPORTED_QUERY_DESCRIPTION);

  ColorfulSymbolContentConfiguration* symbolConfiguration =
      [[ColorfulSymbolContentConfiguration alloc] init];
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
  symbolConfiguration.symbolImage =
      CustomSymbolWithPointSize(kGeminiBrandedLogoSymbol, kIconPointSize);
#else
  symbolConfiguration.symbolImage =
      DefaultSymbolWithPointSize(kGeminiNonBrandedLogoSymbol, kIconPointSize);
#endif
  symbolConfiguration.symbolTintColor = [UIColor colorNamed:kTextPrimaryColor];
  configuration.leadingConfiguration = symbolConfiguration;

  UITableViewCell* cell =
      [TableViewCellContentConfiguration dequeueTableViewCell:tableView];
  cell.contentConfiguration = configuration;
  cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  cell.contentView.alpha = kDefaultCellAlpha;
  cell.userInteractionEnabled = YES;
  cell.accessibilityIdentifier =
      kAtMemoryUnsupportedQueryCellAccessibilityIdentifier;

  return cell;
}

// Returns the table view cell for the notice state.
- (UITableViewCell*)noticeCellForTableView:(UITableView*)tableView {
  UITableViewCell* cell =
      [AtMemoryInlineNoticeConfiguration dequeueTableViewCell:tableView];
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  cell.backgroundColor = [UIColor clearColor];
  cell.contentView.backgroundColor = [UIColor clearColor];

  AtMemoryInlineNoticeConfiguration* config =
      [[AtMemoryInlineNoticeConfiguration alloc] init];
  config.delegate = self;
  cell.contentConfiguration = config;
  return cell;
}

#pragma mark - AtMemoryInlineNoticeViewDelegate

- (void)inlineNoticeViewDidTapOK:(AtMemoryInlineNoticeView*)view {
  // TODO(crbug.com/540433768): Forward to mutator to handle notice dismissal.
}

- (void)inlineNoticeViewDidTapSettings:(AtMemoryInlineNoticeView*)view {
  // TODO(crbug.com/540433768): Forward to mutator to handle settings redirect.
}

@end
