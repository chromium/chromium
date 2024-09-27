// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_browsing_data_view_controller.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "components/browsing_data/core/browsing_data_utils.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/cells/clear_browsing_data_constants.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_browsing_data_view_controller_delegate.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_mutator.h"
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
  SectionIdentifierFooter,
};

// Item identifiers in the browsing data page table view.
typedef NS_ENUM(NSInteger, ItemIdentifier) {
  ItemIdentifierHistory = kItemTypeEnumZero,
  ItemIdentifierTabs,
  ItemIdentifierSiteData,
  ItemIdentifierCache,
  ItemIdentifierPasswords,
  ItemIdentifierAutofill,
};

}  // namespace

@interface QuickDeleteBrowsingDataViewController () <
    TableViewLinkHeaderFooterItemDelegate> {
  UITableViewDiffableDataSource<NSNumber*, NSNumber*>* _dataSource;

  NSString* _historySummary;
  NSString* _tabsSummary;
  NSString* _cacheSummary;
  NSString* _passwordsSummary;
  NSString* _autofillSummary;
  BOOL _historySelected;
  BOOL _tabsSelected;
  BOOL _siteDataSelected;
  BOOL _cacheSelected;
  BOOL _passwordsSelected;
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
  self.navigationItem.rightBarButtonItem = [self confirmButton];
  [self updateConfirmButtonEnabledStatus];
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

  RegisterTableViewCell<TableViewDetailIconCell>(self.tableView);
  RegisterTableViewHeaderFooter<TableViewLinkHeaderFooterView>(self.tableView);

  NSDiffableDataSourceSnapshot* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];
  [snapshot appendSectionsWithIdentifiers:@[
    @(SectionIdentifierBrowsingData), @(SectionIdentifierFooter)
  ]];
  [snapshot appendItemsWithIdentifiers:@[
    @(ItemIdentifierHistory), @(ItemIdentifierTabs), @(ItemIdentifierSiteData),
    @(ItemIdentifierCache), @(ItemIdentifierPasswords),
    @(ItemIdentifierAutofill)
  ]
             intoSectionWithIdentifier:@(SectionIdentifierBrowsingData)];

  [_dataSource applySnapshot:snapshot animatingDifferences:NO];
}

#pragma mark - UITableViewDelegate
- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
  ItemIdentifier itemIdentifier = static_cast<ItemIdentifier>(
      [_dataSource itemIdentifierForIndexPath:indexPath].integerValue);

  // Update selection value for the corresponding cell with `itemIdentifier`.
  [self toggleSelectionForItemIdentifier:itemIdentifier];

  // Update the snapshot for the selected cell.
  [self updateSnapshotForItemIdentifier:itemIdentifier];
}

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  SectionIdentifier sectionIdentifier = static_cast<SectionIdentifier>(
      [_dataSource sectionIdentifierForIndex:section].integerValue);
  switch (sectionIdentifier) {
    case SectionIdentifierFooter: {
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
    case SectionIdentifierBrowsingData: {
      return nil;
    }
  }
  NOTREACHED();
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  SectionIdentifier sectionIdentifier = static_cast<SectionIdentifier>(
      [_dataSource sectionIdentifierForIndex:section].integerValue);
  if (sectionIdentifier == SectionIdentifierFooter && _shouldShowFooter) {
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

- (void)setShouldShowFooter:(BOOL)shouldShowFooter {
  if (_shouldShowFooter == shouldShowFooter) {
    return;
  }

  _shouldShowFooter = shouldShowFooter;

  // Reload the footer section.
  NSDiffableDataSourceSnapshot<NSNumber*, NSNumber*>* snapshot =
      [_dataSource snapshot];
  [snapshot reloadSectionsWithIdentifiers:@[ @(SectionIdentifierFooter) ]];
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

- (void)setPasswordsSummary:(NSString*)passwordsSummary {
  _passwordsSummary = passwordsSummary;
  [self updateSnapshotForItemIdentifier:ItemIdentifierPasswords];
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

- (void)setPasswordsSelection:(BOOL)selected {
  _passwordsSelected = selected;
  [self updateSnapshotForItemIdentifier:ItemIdentifierPasswords];
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

// Updates the enabled status of the confirm button. The confirm button should
// only be enabled if at least one browsing data type is selected for deletion.
- (void)updateConfirmButtonEnabledStatus {
  self.navigationItem.rightBarButtonItem.enabled =
      _historySelected || _tabsSelected || _siteDataSelected ||
      _cacheSelected || _passwordsSelected || _autofillSelected;
}

// Returns the cancel button on the navigation bar.
- (UIBarButtonItem*)cancelButton {
  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(onCancel:)];
  return cancelButton;
}

// Returns the confirm button on the navigation bar.
- (UIBarButtonItem*)confirmButton {
  UIBarButtonItem* confirmButton = [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_DELETE_BROWSING_DATA_CONFIRM)
              style:UIBarButtonItemStyleDone
             target:self
             action:@selector(onConfirm:)];
  confirmButton.accessibilityIdentifier =
      kQuickDeleteBrowsingDataConfirmButtonIdentifier;
  return confirmButton;
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
  [_mutator updatePasswordsSelection:_passwordsSelected];
  [_mutator updateAutofillSelection:_autofillSelected];
  [_delegate dismissBrowsingDataPage];
}

// Creates the browsing data cell.
- (TableViewDetailIconCell*)createCellWithTitle:(NSString*)title
                                        summary:(NSString*)summary
                                           icon:(UIImage*)icon
                                       selected:(BOOL)selected
                        accessibilityIdentifier:
                            (NSString*)accessibilityIdentifier {
  TableViewDetailIconCell* cell =
      DequeueTableViewCell<TableViewDetailIconCell>(self.tableView);
  cell.textLabel.text = title;
  // Placeholder description required by the constraint to avoid cell resize.
  cell.detailText = summary ? summary : @" ";
  cell.textLayoutConstraintAxis = UILayoutConstraintAxisVertical;
  [cell setIconImage:icon
            tintColor:[UIColor colorNamed:kGrey500Color]
      backgroundColor:cell.backgroundColor
         cornerRadius:0];
  cell.accessoryType = selected ? UITableViewCellAccessoryCheckmark
                                : UITableViewCellAccessoryNone;
  cell.accessibilityIdentifier = accessibilityIdentifier;
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
    case ItemIdentifierPasswords: {
      return [self
              createCellWithTitle:l10n_util::GetNSString(
                                      IDS_IOS_CLEAR_SAVED_PASSWORDS)
                          summary:_passwordsSummary
                             icon:[self iconForItemIdentifier:itemIdentifier]
                         selected:_passwordsSelected
          accessibilityIdentifier:kQuickDeleteBrowsingDataPasswordsIdentifier];
    }
    case ItemIdentifierAutofill: {
      return [self
              createCellWithTitle:l10n_util::GetNSString(IDS_IOS_CLEAR_AUTOFILL)
                          summary:_autofillSummary
                             icon:[self iconForItemIdentifier:itemIdentifier]
                         selected:_autofillSelected
          accessibilityIdentifier:kQuickDeleteBrowsingDataAutofillIdentifier];
    }
  }
}

// Reloads the snapshot for the cell with the given `itemIdentifier`.
- (void)updateSnapshotForItemIdentifier:(ItemIdentifier)itemIdentifier {
  NSDiffableDataSourceSnapshot<NSNumber*, NSNumber*>* snapshot =
      [_dataSource snapshot];
  [snapshot reloadItemsWithIdentifiers:@[ @(itemIdentifier) ]];
  [_dataSource applySnapshot:snapshot animatingDifferences:YES];

  [self updateConfirmButtonEnabledStatus];
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
    case ItemIdentifierPasswords: {
      _passwordsSelected = !_passwordsSelected;
      break;
    }
    case ItemIdentifierAutofill: {
      _autofillSelected = !_autofillSelected;
      break;
    }
  }
  [self updateConfirmButtonEnabledStatus];
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
    case ItemIdentifierPasswords: {
      return CustomSymbolTemplateWithPointSize(kPasswordSymbol,
                                               kDefaultSymbolSize);
    }
    case ItemIdentifierAutofill: {
      return DefaultSymbolTemplateWithPointSize(kAutofillDataSymbol,
                                                kDefaultSymbolSize);
    }
  }
}

@end
