// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/clear_browsing_data/quick_delete_browsing_data/ui/quick_delete_browsing_data_view_controller.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "components/browsing_data/core/browsing_data_utils.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/settings/clear_browsing_data/public/quick_delete_constants.h"
#import "ios/chrome/browser/settings/clear_browsing_data/quick_delete_browsing_data/ui/quick_delete_browsing_data_view_controller_delegate.h"
#import "ios/chrome/browser/settings/clear_browsing_data/ui/quick_delete_mutator.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/colorful_symbol_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

using browsing_data::DeleteBrowsingDataDialogAction;

// Browing data type icon size.
const CGFloat kDefaultSymbolSize = 24;

// TableView's footer section height.
constexpr CGFloat kSectionFooterHeight = 0;

// The URL for signing out of Chrome from Delete Browsing Data (DBD).
const char kDBDSignOutOfChromeURL[] = "settings://DBDSignOutOfChrome";

// Section identifiers in the browsing data page table view.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierBrowsingData = kSectionIdentifierEnumZero,
  SectionIdentifierBrowsingDataFooter,
  SectionIdentifierManageOtherData,
};

// Item identifiers in the browsing data page table view.
typedef NS_ENUM(NSInteger, ItemIdentifier) {
  ItemIdentifierHistory = kItemTypeEnumZero,
  ItemIdentifierTabs,
  ItemIdentifierSiteData,
  ItemIdentifierCache,
  ItemIdentifierAutofill,
  ItemIdentifierManageOtherData,
};

// Returns the array of item identifiers for the Browsing Data section.
NSArray<NSNumber*>* BrowsingDataItemIdentifiers() {
  NSMutableArray<NSNumber*>* items = [NSMutableArray array];
  [items addObject:@(ItemIdentifierHistory)];
  [items addObject:@(ItemIdentifierTabs)];
  [items addObject:@(ItemIdentifierSiteData)];
  [items addObject:@(ItemIdentifierCache)];
  [items addObject:@(ItemIdentifierAutofill)];
  return items;
}

}  // namespace

@interface QuickDeleteBrowsingDataViewController () <
    TableViewLinkHeaderFooterItemDelegate> {
  UITableViewDiffableDataSource<NSNumber*, NSNumber*>* _dataSource;

  // The title for the "Manage other data" cell.
  NSString* _manageOtherDataTitle;
  // The subtitle for the "Manage other data" cell.
  NSString* _manageOtherDataSubtitle;
  NSString* _historySummary;
  NSString* _tabsSummary;
  NSString* _cacheSummary;
  NSString* _autofillSummary;
  BOOL _historySelected;
  BOOL _tabsSelected;
  BOOL _siteDataSelected;
  BOOL _cacheSelected;
  BOOL _autofillSelected;
  BOOL _shouldShowFooter;
}
@end

@implementation QuickDeleteBrowsingDataViewController

#pragma mark - ChromeTableViewController

- (instancetype)init {
  UITableViewStyle style = ChromeTableViewStyle();
  return [super initWithStyle:style];
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.navigationItem.largeTitleDisplayMode =
      UINavigationItemLargeTitleDisplayModeNever;
  self.navigationItem.leftBarButtonItem = [self cancelButton];
  self.navigationItem.rightBarButtonItem = [self doneButton];
  [self updateDoneButtonEnabledStatus];
  self.title = l10n_util::GetNSString(IDS_IOS_DELETE_BROWSING_DATA_TITLE);
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

  [TableViewCellContentConfiguration registerCellForTableView:self.tableView];
  RegisterTableViewHeaderFooter<TableViewLinkHeaderFooterView>(self.tableView);

  NSDiffableDataSourceSnapshot* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];
  [snapshot appendSectionsWithIdentifiers:@[
    @(SectionIdentifierBrowsingData), @(SectionIdentifierBrowsingDataFooter)
  ]];
  [snapshot appendItemsWithIdentifiers:BrowsingDataItemIdentifiers()
             intoSectionWithIdentifier:@(SectionIdentifierBrowsingData)];

    [snapshot
        appendSectionsWithIdentifiers:@[ @(SectionIdentifierManageOtherData) ]];
    [snapshot appendItemsWithIdentifiers:@[ @(ItemIdentifierManageOtherData) ]
               intoSectionWithIdentifier:@(SectionIdentifierManageOtherData)];

  [_dataSource applySnapshot:snapshot animatingDifferences:NO];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
  ItemIdentifier itemIdentifier = static_cast<ItemIdentifier>(
      [_dataSource itemIdentifierForIndexPath:indexPath].integerValue);

  switch (itemIdentifier) {
    case ItemIdentifierManageOtherData: {
      [_delegate showOtherDataPage];
      return;
    }
    case ItemIdentifierHistory:
    case ItemIdentifierTabs:
    case ItemIdentifierSiteData:
    case ItemIdentifierCache:
    case ItemIdentifierAutofill:
      // Update selection value for the corresponding cell with
      // `itemIdentifier`.
      [self toggleSelectionForItemIdentifier:itemIdentifier];
      // Update the snapshot for the selected cell.
      [self updateSnapshotForItemIdentifier:itemIdentifier];
  }
}

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  SectionIdentifier sectionIdentifier = static_cast<SectionIdentifier>(
      [_dataSource sectionIdentifierForIndex:section].integerValue);
  switch (sectionIdentifier) {
    case SectionIdentifierBrowsingDataFooter: {
      if (!_shouldShowFooter) {
        return nil;
      }
      TableViewLinkHeaderFooterView* footer =
          DequeueTableViewHeaderFooter<TableViewLinkHeaderFooterView>(
              tableView);
      footer.accessibilityIdentifier = kQuickDeleteBrowsingDataFooterIdentifier;
      footer.delegate = self;
      footer.urls =
          @[ [[CrURL alloc] initWithGURL:GURL(kDBDSignOutOfChromeURL)] ];
      [footer setText:l10n_util::GetNSString(
                          IDS_IOS_DELETE_BROWSING_DATA_PAGE_FOOTER)
            withColor:[UIColor colorNamed:kTextSecondaryColor]];
      return footer;
    }
    case SectionIdentifierBrowsingData:
    case SectionIdentifierManageOtherData:
      return nil;
  }
  NOTREACHED();
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  SectionIdentifier sectionIdentifier = static_cast<SectionIdentifier>(
      [_dataSource sectionIdentifierForIndex:section].integerValue);
  if (sectionIdentifier == SectionIdentifierBrowsingDataFooter &&
      _shouldShowFooter) {
    return UITableViewAutomaticDimension;
  }
  return kSectionFooterHeight;
}

#pragma mark - TableViewLinkHeaderFooterItemDelegate

- (void)view:(TableViewLinkHeaderFooterView*)view didTapLinkURL:(CrURL*)url {
  CHECK(url.gurl == kDBDSignOutOfChromeURL);
  base::UmaHistogramEnumeration(
      browsing_data::kDeleteBrowsingDataDialogHistogram,
      DeleteBrowsingDataDialogAction::kSignoutLinkOpened);
  base::RecordAction(base::UserMetricsAction("ClearBrowsingData_SignOut"));
  [_delegate signOutAndShowActionSheet];
}

#pragma mark - QuickDeleteConsumer

- (void)setTimeRange:(browsing_data::TimePeriod)timeRange {
  // No-op: This ViewController doesn't make user of the selected time range.
}

- (void)setBrowsingDataSummary:(NSString*)summary {
  // No-op: This ViewController doesn't show the overall browsing data summary.
}

- (void)setManageOtherDataTitle:(NSString*)manageOtherDataTitle {
  _manageOtherDataTitle = manageOtherDataTitle;

  // Reloads the "Manage other data" cell.
  [self updateSnapshotForItemIdentifier:ItemIdentifierManageOtherData];
}

- (void)setManageOtherDataSubtitle:(NSString*)manageOtherDataSubtitle {
  _manageOtherDataSubtitle = manageOtherDataSubtitle;

  // Reloads the "Manage other data" cell.
  [self updateSnapshotForItemIdentifier:ItemIdentifierManageOtherData];
}

- (void)setShouldShowFooter:(BOOL)shouldShowFooter {
  if (_shouldShowFooter == shouldShowFooter) {
    return;
  }

  _shouldShowFooter = shouldShowFooter;

  // Reload the footer section.
  NSDiffableDataSourceSnapshot<NSNumber*, NSNumber*>* snapshot =
      [_dataSource snapshot];
  [snapshot reloadSectionsWithIdentifiers:@[
    @(SectionIdentifierBrowsingDataFooter)
  ]];
  [_dataSource applySnapshot:snapshot animatingDifferences:YES];
}

- (void)setHistorySummary:(NSString*)historySummary {
  _historySummary = historySummary;
  [self updateSnapshotForItemIdentifier:ItemIdentifierHistory];
}

- (void)setTabsSummary:(NSString*)tabsSummary {
  _tabsSummary = tabsSummary;
  [self updateSnapshotForItemIdentifier:ItemIdentifierTabs];
}

- (void)setCacheSummary:(NSString*)cacheSummary {
  _cacheSummary = cacheSummary;
  [self updateSnapshotForItemIdentifier:ItemIdentifierCache];
}

- (void)setAutofillSummary:(NSString*)autofillSummary {
  _autofillSummary = autofillSummary;
  [self updateSnapshotForItemIdentifier:ItemIdentifierAutofill];
}

- (void)setHistorySelection:(BOOL)selected {
  _historySelected = selected;
  [self updateSnapshotForItemIdentifier:ItemIdentifierHistory];
}

- (void)setTabsSelection:(BOOL)selected {
  _tabsSelected = selected;
  [self updateSnapshotForItemIdentifier:ItemIdentifierTabs];
}

- (void)setSiteDataSelection:(BOOL)selected {
  _siteDataSelected = selected;
  [self updateSnapshotForItemIdentifier:ItemIdentifierSiteData];
}

- (void)setCacheSelection:(BOOL)selected {
  _cacheSelected = selected;
  [self updateSnapshotForItemIdentifier:ItemIdentifierCache];
}

- (void)setAutofillSelection:(BOOL)selected {
  _autofillSelected = selected;
  [self updateSnapshotForItemIdentifier:ItemIdentifierAutofill];
}

- (void)deletionInProgress {
  NOTREACHED();
}

- (void)deletionFinished {
  NOTREACHED();
}

#pragma mark - Private

// Updates the enabled status of the done button. The done button should
// only be enabled if at least one browsing data type is selected for deletion.
- (void)updateDoneButtonEnabledStatus {
  self.navigationItem.rightBarButtonItem.enabled =
      _historySelected || _tabsSelected || _siteDataSelected ||
      _cacheSelected || _autofillSelected;
}

// Returns the cancel button on the navigation bar.
- (UIBarButtonItem*)cancelButton {
  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(onCancel:)];
  return cancelButton;
}

// Returns the done button on the navigation bar.
- (UIBarButtonItem*)doneButton {
  UIBarButtonItem* doneButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(onConfirm:)];
  doneButton.accessibilityIdentifier =
      kQuickDeleteBrowsingDataDoneButtonIdentifier;
  return doneButton;
}

// Dismisses the page without saving changes in selection.
- (void)onCancel:(id)sender {
  base::UmaHistogramEnumeration(
      browsing_data::kDeleteBrowsingDataDialogHistogram,
      DeleteBrowsingDataDialogAction::kCancelDataTypesSelected);
  [_delegate dismissBrowsingDataPage];
}

// Notifies the mutator of the confirmation of the browsing data types
// selection.
- (void)onConfirm:(id)sender {
  base::UmaHistogramEnumeration(
      browsing_data::kDeleteBrowsingDataDialogHistogram,
      DeleteBrowsingDataDialogAction::kUpdateDataTypesSelected);
  [_mutator updateHistorySelection:_historySelected];
  [_mutator updateTabsSelection:_tabsSelected];
  [_mutator updateSiteDataSelection:_siteDataSelected];
  [_mutator updateCacheSelection:_cacheSelected];
  [_mutator updateAutofillSelection:_autofillSelected];
  [_delegate dismissBrowsingDataPage];
}

// Creates the browsing data cell.
- (UITableViewCell*)createCellWithTitle:(NSString*)title
                                summary:(NSString*)summary
                                   icon:(UIImage*)icon
                               selected:(BOOL)selected
                accessibilityIdentifier:(NSString*)accessibilityIdentifier {
  TableViewCellContentConfiguration* configuration =
      [[TableViewCellContentConfiguration alloc] init];
  configuration.title = title;
  configuration.subtitle = summary ?: @" ";

  ColorfulSymbolContentConfiguration* symbolConfiguration =
      [[ColorfulSymbolContentConfiguration alloc] init];
  symbolConfiguration.symbolImage = icon;
  symbolConfiguration.symbolTintColor = [UIColor colorNamed:kGrey500Color];

  configuration.leadingConfiguration = symbolConfiguration;

  UITableViewCell* cell =
      [TableViewCellContentConfiguration dequeueTableViewCell:self.tableView];

  cell.contentConfiguration = configuration;
  cell.accessibilityIdentifier = accessibilityIdentifier;

  UIAccessibilityTraits traits = cell.accessibilityTraits;
  traits |= UIAccessibilityTraitButton;
  if (selected) {
    cell.accessoryType = UITableViewCellAccessoryCheckmark;
    traits |= UIAccessibilityTraitSelected;
  } else {
    cell.accessoryType = UITableViewCellAccessoryNone;
    traits &= ~UIAccessibilityTraitSelected;
  }
  cell.accessibilityTraits = traits;

  return cell;
}

// Creates the "Manage other data" cell.
- (UITableViewCell*)createManageOtherDataCell {
  TableViewCellContentConfiguration* configuration =
      [[TableViewCellContentConfiguration alloc] init];

  configuration.title = _manageOtherDataTitle;
  configuration.subtitle = _manageOtherDataSubtitle;

  UITableViewCell* cell =
      [TableViewCellContentConfiguration dequeueTableViewCell:self.tableView];

  cell.contentConfiguration = configuration;
  cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;

  cell.accessibilityIdentifier = kQuickDeleteManageOtherDataCellIdentifier;
  cell.accessibilityTraits |= UIAccessibilityTraitButton;

  return cell;
}

// Returns the cell for the corresponding `itemIdentifier`.
- (UITableViewCell*)cellForTableView:(UITableView*)tableView
                           indexPath:(NSIndexPath*)indexPath
                      itemIdentifier:(ItemIdentifier)itemIdentifier {
  switch (itemIdentifier) {
    case ItemIdentifierHistory: {
      return [self
              createCellWithTitle:l10n_util::GetNSString(
                                      IDS_IOS_CLEAR_BROWSING_HISTORY)
                          summary:_historySummary
                             icon:[self iconForItemIdentifier:itemIdentifier]
                         selected:_historySelected
          accessibilityIdentifier:kQuickDeleteBrowsingDataHistoryIdentifier];
    }
    case ItemIdentifierTabs: {
      return [self
              createCellWithTitle:l10n_util::GetNSString(IDS_IOS_CLOSE_TABS)
                          summary:_tabsSummary
                             icon:[self iconForItemIdentifier:itemIdentifier]
                         selected:_tabsSelected
          accessibilityIdentifier:kQuickDeleteBrowsingDataTabsIdentifier];
    }
    case ItemIdentifierSiteData: {
      // Because there is no counter for site data, an explanatory text is
      // displayed.
      return [self
              createCellWithTitle:l10n_util::GetNSString(IDS_IOS_CLEAR_COOKIES)
                          summary:l10n_util::GetNSString(
                                      IDS_DEL_COOKIES_COUNTER)
                             icon:[self iconForItemIdentifier:itemIdentifier]
                         selected:_siteDataSelected
          accessibilityIdentifier:kQuickDeleteBrowsingDataSiteDataIdentifier];
    }
    case ItemIdentifierCache: {
      return [self
              createCellWithTitle:l10n_util::GetNSString(IDS_IOS_CLEAR_CACHE)
                          summary:_cacheSummary
                             icon:[self iconForItemIdentifier:itemIdentifier]
                         selected:_cacheSelected
          accessibilityIdentifier:kQuickDeleteBrowsingDataCacheIdentifier];
    }
    case ItemIdentifierAutofill: {
      return [self
              createCellWithTitle:l10n_util::GetNSString(IDS_IOS_CLEAR_AUTOFILL)
                          summary:_autofillSummary
                             icon:[self iconForItemIdentifier:itemIdentifier]
                         selected:_autofillSelected
          accessibilityIdentifier:kQuickDeleteBrowsingDataAutofillIdentifier];
    }
    case ItemIdentifierManageOtherData: {
      return [self createManageOtherDataCell];
    }
  }
}

// Reloads the snapshot for the cell with the given `itemIdentifier`.
- (void)updateSnapshotForItemIdentifier:(ItemIdentifier)itemIdentifier {
  NSDiffableDataSourceSnapshot<NSNumber*, NSNumber*>* snapshot =
      [_dataSource snapshot];
  [snapshot reloadItemsWithIdentifiers:@[ @(itemIdentifier) ]];
  [_dataSource applySnapshot:snapshot animatingDifferences:YES];

  switch (itemIdentifier) {
    case ItemIdentifierHistory:
    case ItemIdentifierTabs:
    case ItemIdentifierSiteData:
    case ItemIdentifierCache:
    case ItemIdentifierAutofill:
      [self updateDoneButtonEnabledStatus];
      break;
    case ItemIdentifierManageOtherData:
      // Unlike the data type selection cells above, this cell is for
      // navigation. Tapping it doesn't change the enabled state of the done
      // button.
      break;
  }
}

// Toggles the selection for the given `itemIdentifier`.
- (void)toggleSelectionForItemIdentifier:(ItemIdentifier)itemIdentifier {
  switch (itemIdentifier) {
    case ItemIdentifierHistory: {
      _historySelected = !_historySelected;
      break;
    }
    case ItemIdentifierTabs: {
      _tabsSelected = !_tabsSelected;
      break;
    }
    case ItemIdentifierSiteData: {
      _siteDataSelected = !_siteDataSelected;
      break;
    }
    case ItemIdentifierCache: {
      _cacheSelected = !_cacheSelected;
      break;
    }
    case ItemIdentifierAutofill: {
      _autofillSelected = !_autofillSelected;
      break;
    }
    case ItemIdentifierManageOtherData: {
      // This item can't be selected.
      NOTREACHED();
    }
  }
  [self updateDoneButtonEnabledStatus];
}

// Returns the icon for the given `itemIdentifier`.
- (UIImage*)iconForItemIdentifier:(ItemIdentifier)itemIdentifier {
  switch (itemIdentifier) {
    case ItemIdentifierHistory: {
      return DefaultSymbolTemplateWithPointSize(kHistorySymbol,
                                                kDefaultSymbolSize);
    }
    case ItemIdentifierTabs: {
      return DefaultSymbolTemplateWithPointSize(kTabsSymbol,
                                                kDefaultSymbolSize);
    }
    case ItemIdentifierSiteData: {
      return DefaultSymbolTemplateWithPointSize(kInfoCircleSymbol,
                                                kDefaultSymbolSize);
    }
    case ItemIdentifierCache: {
      return DefaultSymbolTemplateWithPointSize(kCachedDataSymbol,
                                                kDefaultSymbolSize);
    }
    case ItemIdentifierAutofill: {
      return DefaultSymbolTemplateWithPointSize(kAutofillDataSymbol,
                                                kDefaultSymbolSize);
    }
    case ItemIdentifierManageOtherData: {
      // This item doesn't have an icon.
      NOTREACHED();
    }
  }
}

@end
