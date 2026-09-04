// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_search_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "build/buildflag.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_commands.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_constants.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_inline_notice_view.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_search_consumer.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_search_item.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_search_mutator.h"
#import "ios/chrome/browser/autofill/atmemory/utils/atmemory_ui_util.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/public/commands/gemini_commands.h"
#import "ios/chrome/browser/shared/ui/buildflags.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/activity_indicator_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/colorful_symbol_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {

// URL for the AI disclosure footer link.
constexpr char kAIDisclosureURL[] = "settings://ai_disclosure";

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

}  // namespace

@interface AtMemorySearchViewController () <
    UISearchBarDelegate,
    UISearchResultsUpdating,
    AtMemoryInlineNoticeViewDelegate,
    TableViewLinkHeaderFooterItemDelegate>
@end

@implementation AtMemorySearchViewController {
  // The table view for this view controller.
  UITableViewDiffableDataSource<NSNumber*, id>* _dataSource;
  // Search controller for users to type a query for performing an AtMemory
  // search and filtering items.
  UISearchController* _searchController;

  // Search results to display in the UI.
  NSArray<AtMemorySearchItem*>* _searchResults;
  // Recent fills to display in the initial empty search state.
  NSArray<AtMemorySearchItem*>* _recentFills;
  // Search query for which the current search results or state was produced.
  NSString* _currentSearchQuery;

  // Tells if the notice is visible.
  BOOL _noticeIsVisible;
  // Tells if the recent fills are visible.
  BOOL _recentFillsAreVisible;
  // The current error type.
  AtMemoryErrorType _errorType;
  // Subtitle to display in the fetching state cell.
  NSString* _fetchingSubtitle;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  _searchController =
      [[UISearchController alloc] initWithSearchResultsController:nil];
  _searchController.obscuresBackgroundDuringPresentation = NO;
  _searchController.hidesNavigationBarDuringPresentation = NO;
  _searchController.searchResultsUpdater = self;
  _searchController.searchBar.delegate = self;
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

  RegisterTableViewHeaderFooter<TableViewLinkHeaderFooterView>(self.tableView);
  RegisterTableViewHeaderFooter<TableViewTextHeaderFooterView>(self.tableView);
  self.tableView.backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
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
                                          id itemIdentifier) {
             if ([itemIdentifier isKindOfClass:[AtMemorySearchItem class]]) {
               return [weakSelf searchItemCellForTableView:tableView
                                                 indexPath:indexPath
                                            itemIdentifier:itemIdentifier];
             }
             return
                 [weakSelf cellForTableView:tableView
                                  indexPath:indexPath
                             itemIdentifier:static_cast<ItemIdentifier>(
                                                [itemIdentifier integerValue])];
           }];
  _dataSource.defaultRowAnimation = UITableViewRowAnimationFade;
  [self createSnapshotForInitialState];
}

#pragma mark - UISearchBarDelegate

- (void)searchBarSearchButtonClicked:(UISearchBar*)searchBar {
  [self startSearchWithQuery:searchBar.text];
}

#pragma mark - UISearchResultsUpdating

- (void)updateSearchResultsForSearchController:
    (UISearchController*)searchController {
  NSString* query = searchController.searchBar.text;

  // Return to the initial state if the search bar is cleared.
  if (query.length == 0) {
    _currentSearchQuery = nil;
    _searchResults = nil;
    [self createSnapshotForInitialState];
    return;
  }

  // Preserve existing search results and avoid resetting the snapshot when
  // UIKit sends search updates for an unchanged query.
  if (_currentSearchQuery && [query isEqualToString:_currentSearchQuery]) {
    return;
  }

  _currentSearchQuery = nil;
  _searchResults = nil;

  BOOL isSearchItemPresent =
      [[_dataSource snapshot]
          indexOfItemIdentifier:@(static_cast<int>(
                                    ItemIdentifier::kSearchItem))] !=
      NSNotFound;

  if (isSearchItemPresent) {
    [self updateSnapshotForItemIdentifier:ItemIdentifier::kSearchItem];
  } else {
    [self createSnapshotForSearchState];
  }
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  id item = [_dataSource itemIdentifierForIndexPath:indexPath];
  if ([item isKindOfClass:[NSNumber class]]) {
    ItemIdentifier itemIdentifier =
        static_cast<ItemIdentifier>([item integerValue]);
    switch (itemIdentifier) {
      case ItemIdentifier::kSearchItem: {
        [self startSearchWithQuery:_searchController.searchBar.text];
        break;
      }
      case ItemIdentifier::kUnsupportedQueryItem: {
        base::RecordAction(
            base::UserMetricsAction("IOS.AtMemory.UnsupportedQueryTapped"));
        [self openGeminiForUnsupportedQuery];
        break;
      }
      case ItemIdentifier::kFetchingItem:
      case ItemIdentifier::kNoDataItem:
      case ItemIdentifier::kNoConnectionItem:
      case ItemIdentifier::kNoticeItem:
        // These cells are not selectable.
        return;
    }
  } else if ([item isKindOfClass:[AtMemorySearchItem class]]) {
    AtMemorySearchItem* searchItem =
        base::apple::ObjCCastStrict<AtMemorySearchItem>(item);
    [self.mutator didSelectSearchResultItem:searchItem];
  }
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

- (UIView*)tableView:(UITableView*)tableView
    viewForHeaderInSection:(NSInteger)section {
  SectionIdentifier sectionIdentifier = static_cast<SectionIdentifier>(
      [_dataSource sectionIdentifierForIndex:section].integerValue);

  if (sectionIdentifier == SectionIdentifier::kRecentFillsSection) {
    TableViewTextHeaderFooterView* header =
        DequeueTableViewHeaderFooter<TableViewTextHeaderFooterView>(tableView);
    [header setTitle:l10n_util::GetNSString(
                         IDS_AUTOFILL_AT_MEMORY_PREVIOUSLY_FILLED)];
    return header;
  }

  return nil;
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  SectionIdentifier sectionIdentifier = static_cast<SectionIdentifier>(
      [_dataSource sectionIdentifierForIndex:section].integerValue);

  if (sectionIdentifier == SectionIdentifier::kRecentFillsSection) {
    return UITableViewAutomaticDimension;
  }

  return 0;
}

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  SectionIdentifier sectionIdentifier = static_cast<SectionIdentifier>(
      [_dataSource sectionIdentifierForIndex:section].integerValue);

  if (sectionIdentifier == SectionIdentifier::kSearchFooterSection) {
    TableViewLinkHeaderFooterView* footer =
        DequeueTableViewHeaderFooter<TableViewLinkHeaderFooterView>(tableView);
    footer.delegate = self;
    footer.urls = @[ [[CrURL alloc] initWithGURL:GURL(kAIDisclosureURL)] ];
    [footer setText:l10n_util::GetNSString(IDS_IOS_AT_MEMORY_AI_DISCLOSURE)
          withColor:[UIColor colorNamed:kTextSecondaryColor]];
    return footer;
  }

  return nil;
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  SectionIdentifier sectionIdentifier = static_cast<SectionIdentifier>(
      [_dataSource sectionIdentifierForIndex:section].integerValue);

  if (sectionIdentifier == SectionIdentifier::kSearchFooterSection) {
    return UITableViewAutomaticDimension;
  }

  return 0;
}

#pragma mark - TableViewLinkHeaderFooterItemDelegate

- (void)view:(TableViewLinkHeaderFooterView*)view didTapLinkURL:(CrURL*)URL {
  if (URL.gurl == GURL(kAIDisclosureURL)) {
    [self.atMemoryHandler openManageEnhancedAutofillDetails];
  }
}

#pragma mark - AtMemoryInlineNoticeViewDelegate

- (void)inlineNoticeViewDidTapOK:(AtMemoryInlineNoticeView*)view {
  [self.mutator acknowledgePrivacyNotice];
}

- (void)inlineNoticeViewDidTapSettings:(AtMemoryInlineNoticeView*)view {
  [self.mutator didTapSettingsLink];
}

#pragma mark - Actions

- (void)handleCancelButton {
  [self.atMemoryHandler dismissAtMemory];
}

- (void)handleInfoButtonTap:(UIButton*)sender {
  [self.mutator openGranularFillForSearchResultAtIndex:sender.tag];
}

#pragma mark - AtMemorySearchConsumer

- (void)setErrorType:(AtMemoryErrorType)errorType {
  _errorType = errorType;
  _currentSearchQuery = [_searchController.searchBar.text copy];
  [self createSnapshotForErrorState];
}

- (void)setNoticeVisible:(BOOL)noticeVisible {
  if (_noticeIsVisible == noticeVisible) {
    return;
  }
  _noticeIsVisible = noticeVisible;
  if (_dataSource) {
    NSDiffableDataSourceSnapshot* snapshot = [_dataSource snapshot];
    NSNumber* noticeSection =
        @(static_cast<int>(SectionIdentifier::kNoticeSection));
    BOOL containsNotice =
        [snapshot.sectionIdentifiers containsObject:noticeSection];

    if (noticeVisible && !containsNotice) {
      // TODO(crbug.com/540433768): Update position of the notice section
      // relative to other sections.
      if (snapshot.sectionIdentifiers.count > 0) {
        [snapshot insertSectionsWithIdentifiers:@[ noticeSection ]
                    beforeSectionWithIdentifier:snapshot.sectionIdentifiers
                                                    .firstObject];
      } else {
        [snapshot appendSectionsWithIdentifiers:@[ noticeSection ]];
      }
      [snapshot appendItemsWithIdentifiers:@[ @(static_cast<int>(
                                               ItemIdentifier::kNoticeItem)) ]
                 intoSectionWithIdentifier:noticeSection];
    } else if (!noticeVisible && containsNotice) {
      [snapshot deleteSectionsWithIdentifiers:@[ noticeSection ]];
    }
    [_dataSource applySnapshot:snapshot animatingDifferences:YES];
  }
  [self updateTableViewBackgroundStyle];
}

- (void)setFetchingSubtitle:(NSString*)subtitle {
  _fetchingSubtitle = [subtitle copy];
  [self updateSnapshotForItemIdentifier:ItemIdentifier::kFetchingItem];
}

- (void)setRecentFills:(NSArray<AtMemorySearchItem*>*)recentFills {
  _recentFills = [recentFills copy];
  _recentFillsAreVisible = _recentFills.count > 0;
  if (_searchController.searchBar.text.length == 0) {
    [self createSnapshotForInitialState];
  }
}

- (void)setSearchResults:(NSArray<AtMemorySearchItem*>*)searchResults {
  _searchResults = searchResults;
  _currentSearchQuery = [_searchController.searchBar.text copy];
  [self createSnapshotForSearchResultsState];
}

#pragma mark - Private

// Initiates the Gemini entry flow for an unsupported query and dismisses the
// AtMemory UI upon success.
- (void)openGeminiForUnsupportedQuery {
  GeminiStartupState* startupState = [[GeminiStartupState alloc]
      initWithEntryPoint:gemini::EntryPoint::AtMemorySearch];
  startupState.prepopulatedPrompt = _searchController.searchBar.text;
  __weak __typeof(self) weakSelf = self;
  [self.geminiHandler
      startGeminiEntryFlowWithStartupState:startupState
                        baseViewController:self
                  showSnackbarOnCompletion:NO
                                completion:^(GeminiEntryFlowResult result) {
                                  if (result == kGeminiEntryFlowResultSuccess) {
                                    [weakSelf.atMemoryHandler dismissAtMemory];
                                  }
                                }];
}

// Creates the `snapshot` for the initial state.
- (void)createSnapshotForInitialState {
  NSDiffableDataSourceSnapshot* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];

  [self appendNoticeSectionToSnapshot:snapshot];

  if (_recentFillsAreVisible) {
    [snapshot appendSectionsWithIdentifiers:@[
      @(static_cast<int>(SectionIdentifier::kRecentFillsSection))
    ]];
    [snapshot appendItemsWithIdentifiers:_recentFills
               intoSectionWithIdentifier:
                   @(static_cast<int>(SectionIdentifier::kRecentFillsSection))];
  }

  [_dataSource applySnapshot:snapshot animatingDifferences:YES];
  [self updateTableViewBackgroundStyle];
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
  [self appendNoticeSectionToSnapshot:snapshot];

  [_dataSource applySnapshot:snapshot animatingDifferences:YES];
  [self updateTableViewBackgroundStyle];
}

// Creates the diffable data source snapshot for the search state.
- (void)createSnapshotForSearchState {
  NSDiffableDataSourceSnapshot* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];
  [snapshot appendSectionsWithIdentifiers:@[
    @(static_cast<int>(SectionIdentifier::kSearchSection))
  ]];
  [snapshot appendItemsWithIdentifiers:@[ @(static_cast<int>(
                                           ItemIdentifier::kSearchItem)) ]
             intoSectionWithIdentifier:@(static_cast<int>(
                                           SectionIdentifier::kSearchSection))];
  [snapshot appendSectionsWithIdentifiers:@[
    @(static_cast<int>(SectionIdentifier::kSearchFooterSection))
  ]];

  [self appendNoticeSectionToSnapshot:snapshot];
  [_dataSource applySnapshot:snapshot animatingDifferences:YES];
  [self updateTableViewBackgroundStyle];
}

// Populates `snapshot` for the fetching state.
- (void)createSnapshotForFetchingState {
  NSDiffableDataSourceSnapshot* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];

  [snapshot appendSectionsWithIdentifiers:@[
    @(static_cast<int>(SectionIdentifier::kFetchingSection))
  ]];
  [snapshot
      appendItemsWithIdentifiers:@[ @(static_cast<int>(
                                     ItemIdentifier::kFetchingItem)) ]
       intoSectionWithIdentifier:@(static_cast<int>(
                                     SectionIdentifier::kFetchingSection))];

  [self appendNoticeSectionToSnapshot:snapshot];
  [_dataSource applySnapshot:snapshot animatingDifferences:YES];
  [self updateTableViewBackgroundStyle];
}

// Populates `snapshot` for the search results state.
- (void)createSnapshotForSearchResultsState {
  NSDiffableDataSourceSnapshot* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];

  [self appendNoticeSectionToSnapshot:snapshot];
  [snapshot appendSectionsWithIdentifiers:@[
    @(static_cast<int>(SectionIdentifier::kSearchResultsSection))
  ]];
  [snapshot appendItemsWithIdentifiers:_searchResults
             intoSectionWithIdentifier:
                 @(static_cast<int>(SectionIdentifier::kSearchResultsSection))];

  [_dataSource applySnapshot:snapshot animatingDifferences:YES];
  [self updateTableViewBackgroundStyle];
}

// Appends the notice section and item to `snapshot` if the notice is visible.
- (void)appendNoticeSectionToSnapshot:(NSDiffableDataSourceSnapshot*)snapshot {
  if (!_noticeIsVisible) {
    return;
  }
  [snapshot appendSectionsWithIdentifiers:@[
    @(static_cast<int>(SectionIdentifier::kNoticeSection))
  ]];
  [snapshot appendItemsWithIdentifiers:@[ @(static_cast<int>(
                                           ItemIdentifier::kNoticeItem)) ]
             intoSectionWithIdentifier:@(static_cast<int>(
                                           SectionIdentifier::kNoticeSection))];
}

// Displays the empty state background if the table view has no data to display.
- (void)updateTableViewBackgroundStyle {
  if (!_dataSource) {
    return;
  }

  if (_dataSource.snapshot.sectionIdentifiers.count == 0) {
    [self setEmptyTableViewBackground];
    return;
  }
  self.tableView.backgroundView = nil;
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
  NSDiffableDataSourceSnapshot<NSNumber*, id>* snapshot =
      [_dataSource snapshot];
  if ([snapshot indexOfItemIdentifier:@(static_cast<int>(itemIdentifier))] ==
      NSNotFound) {
    return;
  }
  [snapshot
      reconfigureItemsWithIdentifiers:@[ @(static_cast<int>(itemIdentifier)) ]];
  [_dataSource applySnapshot:snapshot animatingDifferences:NO];
}

// Returns the cell for the corresponding `itemIdentifier`.
- (UITableViewCell*)cellForTableView:(UITableView*)tableView
                           indexPath:(NSIndexPath*)indexPath
                      itemIdentifier:(ItemIdentifier)itemIdentifier {
  UITableViewCell* cell = nil;
  switch (itemIdentifier) {
    case ItemIdentifier::kSearchItem:
      cell = [self searchCellForTableView:tableView];
      break;
    case ItemIdentifier::kNoticeItem:
      cell = [self noticeCellForTableView:tableView];
      break;
    case ItemIdentifier::kFetchingItem:
      cell = [self fetchingCellForTableView:tableView];
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

// Returns the table view cell for an AtMemory search result item.
- (UITableViewCell*)searchItemCellForTableView:(UITableView*)tableView
                                     indexPath:(NSIndexPath*)indexPath
                                itemIdentifier:
                                    (AtMemorySearchItem*)itemIdentifier {
  TableViewCellContentConfiguration* configuration =
      [[TableViewCellContentConfiguration alloc] init];

  configuration.title = itemIdentifier.title;
  configuration.titleNumberOfLines = 2;
  configuration.titleLineBreakMode = NSLineBreakByTruncatingTail;
  configuration.titleColor = [UIColor colorNamed:kTextPrimaryColor];

  configuration.subtitle = itemIdentifier.subtitle;
  configuration.subtitleNumberOfLines = 1;
  configuration.subtitleLineBreakMode = NSLineBreakByTruncatingTail;

  if (itemIdentifier.icon) {
    ColorfulSymbolContentConfiguration* symbolConfiguration =
        [[ColorfulSymbolContentConfiguration alloc] init];
    symbolConfiguration.symbolImage = itemIdentifier.icon;
    symbolConfiguration.symbolTintColor =
        [UIColor colorNamed:kTextSecondaryColor];
    configuration.leadingConfiguration = symbolConfiguration;
  }

  UITableViewCell* cell =
      [TableViewCellContentConfiguration dequeueTableViewCell:tableView];
  cell.contentConfiguration = configuration;
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  cell.accessibilityIdentifier =
      GetAtMemorySearchResultCellAccessibilityIdentifier(itemIdentifier.title);

  cell.accessoryView = [self infoButtonForSearchItem:itemIdentifier];

  return cell;
}

// Returns a configured info button for the given search result `item`.
- (UIButton*)infoButtonForSearchItem:(AtMemorySearchItem*)item {
  UIButton* infoButton = [UIButton buttonWithType:UIButtonTypeInfoLight];
  [infoButton setImage:SymbolWithPointSize(SymbolInfoCircle, kIconPointSize)
              forState:UIControlStateNormal];
  infoButton.tintColor = [UIColor colorNamed:kBlueColor];
  infoButton.tag = item.index;
  infoButton.accessibilityIdentifier =
      GetAtMemorySearchResultInfoButtonAccessibilityIdentifier(item.title);
  [infoButton addTarget:self
                 action:@selector(handleInfoButtonTap:)
       forControlEvents:UIControlEventTouchUpInside];
  return infoButton;
}

// Returns the table view cell for the "Search" state.
- (UITableViewCell*)searchCellForTableView:(UITableView*)tableView {
  TableViewCellContentConfiguration* configuration =
      [[TableViewCellContentConfiguration alloc] init];
  configuration.title = _searchController.searchBar.text;
  configuration.subtitle =
      l10n_util::GetNSString(IDS_AUTOFILL_AT_MEMORY_SEARCH_AFFORDANCE_SUBTITLE);

  ColorfulSymbolContentConfiguration* iconConfiguration =
      [[ColorfulSymbolContentConfiguration alloc] init];
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
  iconConfiguration.symbolImage =
      SymbolTemplateWithPointSize(SymbolGeminiBrandedLogo, kIconPointSize);
#else
  iconConfiguration.symbolImage =
      SymbolTemplateWithPointSize(SymbolGeminiNonBrandedLogo, kIconPointSize);
#endif
  iconConfiguration.symbolTintColor = [UIColor colorNamed:kTextPrimaryColor];

  configuration.leadingConfiguration = iconConfiguration;

  UITableViewCell* cell =
      [TableViewCellContentConfiguration dequeueTableViewCell:tableView];
  cell.contentConfiguration = configuration;
  cell.accessibilityIdentifier = kAtMemorySearchCellAccessibilityIdentifier;

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
      SymbolWithPointSize(SymbolErrorCircle, kIconPointSize);
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
      SymbolWithPointSize(SymbolErrorCircle, kIconPointSize);
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
      SymbolWithPointSize(SymbolGeminiBrandedLogo, kIconPointSize);
#else
  symbolConfiguration.symbolImage =
      SymbolWithPointSize(SymbolGeminiNonBrandedLogo, kIconPointSize);
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

// Returns the table view cell for the fetching state.
- (UITableViewCell*)fetchingCellForTableView:(UITableView*)tableView {
  TableViewCellContentConfiguration* configuration =
      [[TableViewCellContentConfiguration alloc] init];
  configuration.title = _searchController.searchBar.text;
  configuration.titleColor = [UIColor colorNamed:kTextPrimaryColor];
  configuration.subtitle =
      _fetchingSubtitle
          ?: l10n_util::GetNSString(
                 IDS_AUTOFILL_AT_MEMORY_FETCHING_FINDING_INFO_WITH_GEMINI);

  ActivityIndicatorContentConfiguration* activityIndicatorConfiguration =
      [[ActivityIndicatorContentConfiguration alloc] init];
  configuration.leadingConfiguration = activityIndicatorConfiguration;

  UITableViewCell* cell =
      [TableViewCellContentConfiguration dequeueTableViewCell:tableView];
  cell.contentConfiguration = configuration;
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  cell.userInteractionEnabled = NO;
  cell.accessibilityIdentifier = kAtMemoryFetchingCellAccessibilityIdentifier;

  return cell;
}

// Returns the table view cell for the notice state.
- (UITableViewCell*)noticeCellForTableView:(UITableView*)tableView {
  UITableViewCell* cell =
      [AtMemoryInlineNoticeConfiguration dequeueTableViewCell:tableView];
  cell.selectionStyle = UITableViewCellSelectionStyleNone;

  UIBackgroundConfiguration* backgroundConfiguration =
      [UIBackgroundConfiguration listCellConfiguration];
  backgroundConfiguration.backgroundColor =
      [UIColor colorNamed:kGroupedSecondaryBackgroundColor];
  cell.backgroundConfiguration = backgroundConfiguration;

  AtMemoryInlineNoticeConfiguration* config =
      [[AtMemoryInlineNoticeConfiguration alloc] init];
  config.delegate = self;
  cell.contentConfiguration = config;
  return cell;
}

// Starts an AtMemory search for `query` and transitions to the fetching state.
- (void)startSearchWithQuery:(NSString*)query {
  _currentSearchQuery = [query copy];
  _fetchingSubtitle = l10n_util::GetNSString(
      IDS_AUTOFILL_AT_MEMORY_FETCHING_FINDING_INFO_WITH_GEMINI);
  [self createSnapshotForFetchingState];
  [self.mutator startSearchWithQuery:query];
}

@end
