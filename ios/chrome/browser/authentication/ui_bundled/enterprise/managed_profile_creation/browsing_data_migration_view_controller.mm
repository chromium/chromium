// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/enterprise/managed_profile_creation/browsing_data_migration_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
CGFloat constexpr kTableViewSeparatorInsetHide = 10000;
CGFloat constexpr kSymbolImagePointSize = 17.;
CGFloat constexpr kSectionHeaderHeight = 60;

// Section identifiers in the browsing data page table view.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierBrowsingData = kSectionIdentifierEnumZero,
};

// Item identifiers in the browsing data page table view.
typedef NS_ENUM(NSInteger, ItemIdentifier) {
  ItemIdentifierKeepBrowsingDataSeparate = kItemTypeEnumZero,
  ItemIdentifierMergeBrowsingData,
};
}  // namespace

@interface BrowsingDataMigrationViewController () <UITableViewDelegate>
@end

@implementation BrowsingDataMigrationViewController {
  BOOL _browsingDataSeparate;
  NSString* _userEmail;
  UITableViewDiffableDataSource<NSNumber*, NSNumber*>* _dataSource;
}

- (instancetype)initWithUserEmail:(NSString*)userEmail
             browsingDataSeparate:(BOOL)browsingDataSeparate {
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _browsingDataSeparate = browsingDataSeparate;
    _userEmail = userEmail;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.accessibilityIdentifier =
      kBrowsingDataManagementScreenAccessibilityIdentifier;
  self.view.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
  self.title = l10n_util::GetNSString(
      IDS_IOS_ENTERPRISE_PROFILE_CREATION_ACCOUNT_KEEP_BROWSING_DATA_LABEL);
  self.navigationItem.largeTitleDisplayMode =
      UINavigationItemLargeTitleDisplayModeNever;
  self.tableView.estimatedSectionHeaderHeight = kSectionHeaderHeight;
  self.tableView.sectionHeaderHeight = kSectionHeaderHeight;

  [self loadBrowsingDataTableModel];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:YES];
  [self.tableView
      selectRowAtIndexPath:
          [_dataSource indexPathForItemIdentifier:
                           _browsingDataSeparate
                               ? @(ItemIdentifierKeepBrowsingDataSeparate)
                               : @(ItemIdentifierMergeBrowsingData)]
                  animated:YES
            scrollPosition:UITableViewScrollPositionNone];
}

#pragma mark - UITableViewDelegate

- (NSIndexPath*)tableView:(UITableView*)tableView
    willSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  auto selectedItemIdentifier =
      [_dataSource itemIdentifierForIndexPath:indexPath];
  _browsingDataSeparate = [selectedItemIdentifier
      isEqual:@(ItemIdentifierKeepBrowsingDataSeparate)];
  [self.mutator updateShouldKeepBrowsingDataSeparate:_browsingDataSeparate];
  [self updateSelection];
  return indexPath;
}

- (UIView*)tableView:(UITableView*)tableView
    viewForHeaderInSection:(NSInteger)section {
  TableViewTextHeaderFooterView* view =
      DequeueTableViewHeaderFooter<TableViewTextHeaderFooterView>(
          self.tableView);
  [view
      setSubtitle:
          l10n_util::GetNSString(
              IDS_IOS_ENTERPRISE_PROFILE_CREATION_ACCOUNT_KEEP_BROWSING_DATA_SEPARATE_PAGE_SUBTITLE)];
  return view;
}

#pragma mark - Private

// Creates the Cell that allows the user to select how to handle browsing data.
- (TableViewInfoButtonCell*)
    createBrowsingDataMigrationCellItem:(NSString*)title
                                details:(NSString*)details
                               selected:(BOOL)selected
                accessibilityIdentifier:(NSString*)accessibilityIdentifier {
  TableViewInfoButtonCell* cell =
      DequeueTableViewCell<TableViewInfoButtonCell>(self.tableView);

  cell.accessoryType = UITableViewCellAccessoryNone;
  cell.textLabel.text = title;
  cell.detailTextLabel.text = details;
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  cell.backgroundColor = selected
                             ? [UIColor colorNamed:kBlueHaloColor]
                             : [UIColor colorNamed:kPrimaryBackgroundColor];
  cell.separatorInset =
      UIEdgeInsetsMake(0.f, kTableViewSeparatorInsetHide, 0.f, 0.f);
  cell.accessibilityIdentifier = accessibilityIdentifier;

  UIImageConfiguration* configuration = [UIImageSymbolConfiguration
      configurationWithPointSize:kSymbolImagePointSize
                          weight:UIImageSymbolWeightSemibold
                           scale:UIImageSymbolScaleMedium];
  [cell setIconImage:DefaultSymbolWithConfiguration(kCheckmarkSymbol,
                                                    configuration)
            tintColor:selected ? [UIColor colorNamed:kBlueColor]
                               : [UIColor clearColor]
      backgroundColor:[UIColor clearColor]
         cornerRadius:0.f];
  [cell hideUIButton:YES];

  return cell;
}

// Initializes the table view data model.
- (void)loadBrowsingDataTableModel {
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

  RegisterTableViewCell<TableViewInfoButtonCell>(self.tableView);
  RegisterTableViewHeaderFooter<TableViewTextHeaderFooterView>(self.tableView);

  NSDiffableDataSourceSnapshot* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];
  [snapshot
      appendSectionsWithIdentifiers:@[ @(SectionIdentifierBrowsingData) ]];
  [snapshot appendItemsWithIdentifiers:@[
    @(ItemIdentifierKeepBrowsingDataSeparate),
    @(ItemIdentifierMergeBrowsingData)
  ]];

  [_dataSource applySnapshot:snapshot animatingDifferences:NO];
}

// Returns the cell for the corresponding `itemIdentifier`.
- (UITableViewCell*)cellForTableView:(UITableView*)tableView
                           indexPath:(NSIndexPath*)indexPath
                      itemIdentifier:(ItemIdentifier)itemIdentifier {
  switch (itemIdentifier) {
    case ItemIdentifierKeepBrowsingDataSeparate: {
      auto title = l10n_util::GetNSString(
          IDS_IOS_ENTERPRISE_PROFILE_CREATION_ACCOUNT_KEEP_BROWSING_DATA_SEPARATE_TITLE);
      auto details = l10n_util::GetNSString(
          IDS_IOS_ENTERPRISE_PROFILE_CREATION_ACCOUNT_KEEP_BROWSING_DATA_SEPARATE_SUBTITLE);
      return [self
          createBrowsingDataMigrationCellItem:title
                                      details:details
                                     selected:_browsingDataSeparate
                      accessibilityIdentifier:kKeepBrowsingDataSeparateCellId];
    }
    case ItemIdentifierMergeBrowsingData: {
      auto title = l10n_util::GetNSString(
          IDS_IOS_ENTERPRISE_PROFILE_CREATION_ACCOUNT_MERGE_BROWSING_DATA_TITLE);
      auto details = l10n_util::GetNSStringF(
          IDS_IOS_ENTERPRISE_PROFILE_CREATION_ACCOUNT_MERGE_BROWSING_DATA_SUBTITLE,
          base::SysNSStringToUTF16(_userEmail));
      return
          [self createBrowsingDataMigrationCellItem:title
                                            details:details
                                           selected:!_browsingDataSeparate
                            accessibilityIdentifier:kMergeBrowsingDataCellId];
    }
  }
  NOTREACHED();
}

// Reconfigures every item in the table to show the right checkbox next to the
// right item.
- (void)updateSelection {
  NSDiffableDataSourceSnapshot<NSNumber*, NSNumber*>* snapshot =
      [_dataSource snapshot];
  [snapshot reconfigureItemsWithIdentifiers:@[
    @(ItemIdentifierKeepBrowsingDataSeparate),
    @(ItemIdentifierMergeBrowsingData)
  ]];
  [_dataSource applySnapshot:snapshot animatingDifferences:YES];
}

@end
