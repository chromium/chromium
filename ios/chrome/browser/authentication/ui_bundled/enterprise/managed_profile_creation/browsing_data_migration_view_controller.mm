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
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/colorful_symbol_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
CGFloat constexpr kSymbolImagePointSize = 17.;
CGFloat constexpr kSeparatorInset = 60;

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

  self.tableView.separatorInset = UIEdgeInsetsMake(0, kSeparatorInset, 0, 0);

  self.view.accessibilityIdentifier =
      kBrowsingDataManagementScreenAccessibilityIdentifier;
  self.view.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
  self.title = l10n_util::GetNSString(
      IDS_IOS_ENTERPRISE_PROFILE_CREATION_ACCOUNT_KEEP_BROWSING_DATA_LABEL);
  self.navigationItem.largeTitleDisplayMode =
      UINavigationItemLargeTitleDisplayModeNever;

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

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  return UITableViewAutomaticDimension;
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
- (UITableViewCell*)createBrowsingDataMigrationCellItem:(NSString*)title
                                                details:(NSString*)details
                                               selected:(BOOL)selected
                                accessibilityIdentifier:
                                    (NSString*)accessibilityIdentifier {
  TableViewCellContentConfiguration* configuration =
      [[TableViewCellContentConfiguration alloc] init];

  configuration.title = title;
  configuration.subtitle = details;

  ColorfulSymbolContentConfiguration* checkmarkConfig =
      [[ColorfulSymbolContentConfiguration alloc] init];
  UIImageConfiguration* symbolConfiguration = [UIImageSymbolConfiguration
      configurationWithPointSize:kSymbolImagePointSize
                          weight:UIImageSymbolWeightSemibold
                           scale:UIImageSymbolScaleMedium];
  checkmarkConfig.symbolImage =
      DefaultSymbolWithConfiguration(kCheckmarkSymbol, symbolConfiguration);
  checkmarkConfig.symbolTintColor =
      selected ? [UIColor colorNamed:kBlueColor] : [UIColor clearColor];

  configuration.leadingConfiguration = checkmarkConfig;

  UITableViewCell* cell =
      [TableViewCellContentConfiguration dequeueTableViewCell:self.tableView];

  cell.contentConfiguration = configuration;

  cell.accessoryType = UITableViewCellAccessoryNone;
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  cell.backgroundColor = selected
                             ? [UIColor colorNamed:kBlueHaloColor]
                             : [UIColor colorNamed:kPrimaryBackgroundColor];
  cell.accessibilityIdentifier = accessibilityIdentifier;

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

  [TableViewCellContentConfiguration registerCellForTableView:self.tableView];
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
