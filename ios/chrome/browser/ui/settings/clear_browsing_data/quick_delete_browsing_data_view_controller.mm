// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_browsing_data_view_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "components/browsing_data/core/browsing_data_utils.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/cells/clear_browsing_data_constants.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_browsing_data_view_controller_delegate.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_mutator.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Browing data type icon size.
const CGFloat kDefaultSymbolSize = 24;

// Section identifiers in the browsing data page table view.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierBrowsingData = kSectionIdentifierEnumZero,
};

// Item identifiers in the browsing data page table view.
typedef NS_ENUM(NSInteger, ItemIdentifier) {
  ItemIdentifierHistory = kItemTypeEnumZero,
  ItemIdentifierSiteData,
  ItemIdentifierAutofill,
};

}  // namespace

@implementation QuickDeleteBrowsingDataViewController {
  UITableViewDiffableDataSource<NSNumber*, NSNumber*>* _dataSource;
  NSString* _historySummary;
  NSString* _autofillSummary;
  BOOL _historySelected;
  BOOL _siteDataSelected;
  BOOL _autofillSelected;
}

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

  NSDiffableDataSourceSnapshot* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];
  [snapshot
      appendSectionsWithIdentifiers:@[ @(SectionIdentifierBrowsingData) ]];
  [snapshot appendItemsWithIdentifiers:@[
    @(ItemIdentifierHistory), @(ItemIdentifierSiteData),
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
  [self updateSelectionForItemIdentifier:itemIdentifier];

  // Update the snapshot for the selected cell.
  [self updateSnapshotForItemIdentifier:itemIdentifier];
}

#pragma mark - QuickDeleteConsumer

- (void)setTimeRange:(browsing_data::TimePeriod)timeRange {
  // TODO(crbug.com/341107834): Decide whether this is required to be
  // implemented here or skipped.
}

- (void)setBrowsingDataSummary:(NSString*)summary {
  // TODO(crbug.com/353211728): Remove this after refactoring the main page
  // summary to use the new methods for results & selection.
}

- (void)setShouldShowFooter:(BOOL)shouldShowFooter {
  // TODO(crbug.com/341107834): Store the boolean value to used for the footer.
}

- (void)updateHistoryWithResult:
    (const browsing_data::BrowsingDataCounter::Result&)result {
  _historySummary = [self counterTextFromResult:result];
  [self updateSnapshotForItemIdentifier:ItemIdentifierHistory];
}

- (void)updateAutofillWithResult:
    (const browsing_data::BrowsingDataCounter::Result&)result {
  _autofillSummary = [self counterTextFromResult:result];
  [self updateSnapshotForItemIdentifier:ItemIdentifierAutofill];
}

- (void)setHistorySelection:(BOOL)selected {
  _historySelected = selected;
  [self updateSnapshotForItemIdentifier:ItemIdentifierHistory];
}

- (void)setSiteDataSelection:(BOOL)selected {
  _siteDataSelected = selected;
  [self updateSnapshotForItemIdentifier:ItemIdentifierSiteData];
}

- (void)setAutofillSelection:(BOOL)selected {
  _autofillSelected = selected;
  [self updateSnapshotForItemIdentifier:ItemIdentifierAutofill];
}

- (void)deletionInProgress {
  NOTREACHED_NORETURN();
}

- (void)deletionFinished {
  NOTREACHED_NORETURN();
}

#pragma mark - Private

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
              style:UIBarButtonItemStylePlain
             target:self
             action:@selector(onConfirm:)];
  confirmButton.accessibilityIdentifier =
      kQuickDeleteBrowsingDataConfirmButtonIdentifier;
  return confirmButton;
}

// Dismisses the page without saving changes in selection.
- (void)onCancel:(id)sender {
  [_delegate dismissBrowsingDataPage];
}

// Notifies the mutator of the confirmation of the browsing data types
// selection.
- (void)onConfirm:(id)sender {
  [_mutator updateHistorySelection:_historySelected];
  [_mutator updateSiteDataSelection:_siteDataSelected];
  [_mutator updateAutofillSelection:_autofillSelected];
  // TODO(crbug.com/341107834): Update changes in data types selection here.
  [_delegate dismissBrowsingDataPage];
}

// Returns the appropriate summary subtitle for the given counter result.
// TODO(crbug.com/341107834): Move this to a helper util file and implement
// cache & tabs types string handling as it's different on iOS than other
// platforms.
- (NSString*)counterTextFromResult:
    (const browsing_data::BrowsingDataCounter::Result&)result {
  return base::SysUTF16ToNSString(
      browsing_data::GetCounterTextFromResult(&result));
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
}

// Toggles the selection for the given `itemIdentifier`.
- (void)updateSelectionForItemIdentifier:(ItemIdentifier)itemIdentifier {
  switch (itemIdentifier) {
    case ItemIdentifierHistory: {
      _historySelected = !_historySelected;
      break;
    }
    case ItemIdentifierSiteData: {
      _siteDataSelected = !_siteDataSelected;
      break;
    }
    case ItemIdentifierAutofill: {
      _autofillSelected = !_autofillSelected;
      break;
    }
      // TODO(crbug.com/341107834): Update other data types selection state
      // here.
  }
}

// Returns the icon for the given `itemIdentifier`.
- (UIImage*)iconForItemIdentifier:(ItemIdentifier)itemIdentifier {
  switch (itemIdentifier) {
    case ItemIdentifierHistory: {
      return DefaultSymbolTemplateWithPointSize(kHistorySymbol,
                                                kDefaultSymbolSize);
    }
    case ItemIdentifierSiteData: {
      return DefaultSymbolTemplateWithPointSize(kInfoCircleSymbol,
                                                kDefaultSymbolSize);
    }
    case ItemIdentifierAutofill: {
      return DefaultSymbolTemplateWithPointSize(kAutofillDataSymbol,
                                                kDefaultSymbolSize);
    }
      // TODO(crbug.com/341107834): Add other icons for remaining types here.
  }
}

@end
