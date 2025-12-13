// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/data_import/ui/data_import_invalid_passwords_view_controller.h"

#import "base/check_op.h"
#import "ios/chrome/browser/data_import/public/accessibility_utils.h"
#import "ios/chrome/browser/data_import/public/password_import_item.h"
#import "ios/chrome/browser/data_import/ui/password_import_item_cell_content_configuration.h"
#import "ios/chrome/browser/data_import/ui/ui_utils.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_attributed_string_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
/// The identifier for the only section in the table.
NSString* const kDataImportInvalidPasswordSection =
    @"kDataImportInvalidPasswordSection";
}  // namespace

@interface DataImportInvalidPasswordsViewController () <UITableViewDelegate>

@end

@implementation DataImportInvalidPasswordsViewController {
  /// List of invalid passwords.
  NSArray<PasswordImportItem*>* _invalidPasswords;
  /// The data source painting each cell in the table from `_invalidPasswords`.
  UITableViewDiffableDataSource<NSString*, NSNumber*>* _dataSource;
}

#pragma mark - ChromeTableViewController

- (instancetype)initWithInvalidPasswords:
    (NSArray<PasswordImportItem*>*)passwords {
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _invalidPasswords = passwords;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  NSString* closeButtonTitle = l10n_util::GetNSString(
      IDS_IOS_SAFARI_IMPORT_INVALID_PASSWORD_LIST_BUTTON_CLOSE);
  self.title =
      l10n_util::GetNSString(IDS_IOS_SAFARI_IMPORT_INVALID_PASSWORD_LIST_TITLE);
  UIBarButtonItem* closeButton =
      [[UIBarButtonItem alloc] initWithTitle:closeButtonTitle
                                       style:UIBarButtonItemStylePlain
                                      target:self
                                      action:@selector(didTapCloseButton)];
  self.navigationItem.rightBarButtonItem = closeButton;
  /// Sets up table view properties.
  self.tableView.accessibilityIdentifier =
      GetInvalidPasswordsTableViewAccessibilityIdentifier();
  self.tableView.delegate = self;
  self.tableView.allowsSelection = NO;
  self.tableView.separatorInset =
      GetDataImportSeparatorInset(/*multiSelectionMode=*/NO);
  /// Register cells.
  RegisterTableViewCell<UITableViewCell>(self.tableView);
  RegisterTableViewHeaderFooter<TableViewAttributedStringHeaderFooterView>(
      self.tableView);
  /// Initialize table.
  [self initializeDataSourceAndTable];
}

#pragma mark - UITableViewDelegate

- (UIView*)tableView:(UITableView*)tableView
    viewForHeaderInSection:(NSInteger)section {
  TableViewAttributedStringHeaderFooterView* header =
      DequeueTableViewHeaderFooter<TableViewAttributedStringHeaderFooterView>(
          tableView);
  NSDictionary* attributes = @{
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote],
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor]
  };
  NSMutableAttributedString* attributedText = [[NSMutableAttributedString alloc]
      initWithString:
          l10n_util::GetNSString(
              IDS_IOS_SAFARI_IMPORT_INVALID_PASSWORD_LIST_DESCRIPTION)
          attributes:attributes];
  [header setAttributedString:attributedText];
  return header;
}

#pragma mark - Private

/// Handler for the top-trailing "Close" button.
- (void)didTapCloseButton {
  [self.presentingViewController dismissViewControllerAnimated:YES
                                                    completion:nil];
}

/// Returns the cell with the properties of the `item` displayed.
- (UITableViewCell*)cellForIndexPath:(NSIndexPath*)indexPath
                      itemIdentifier:(NSNumber*)identifier {
  PasswordImportItem* item = _invalidPasswords[identifier.intValue];
  PasswordImportItemCellContentConfiguration* config =
      [PasswordImportItemCellContentConfiguration
          cellConfigurationForErrorMessage:item];
  if (item.faviconAttributes) {
    config.faviconAttributes = item.faviconAttributes;
  } else {
    __weak __typeof(self) weakSelf = self;
    [item loadFaviconWithUIUpdateHandler:^{
      [weakSelf updateItemWithIdentifier:identifier];
    }];
  }
  UITableViewCell* cell = DequeueTableViewCell<UITableViewCell>(self.tableView);
  cell.accessibilityIdentifier =
      GetInvalidPasswordsTableViewCellAccessibilityIdentifier(indexPath.item);
  cell.contentConfiguration = config;
  return cell;
}

/// Helper method to update the cell with `identifier`.
- (void)updateItemWithIdentifier:(NSNumber*)identifier {
  NSDiffableDataSourceSnapshot<NSString*, NSNumber*>* snapshot =
      [_dataSource snapshot];
  [snapshot reconfigureItemsWithIdentifiers:@[ identifier ]];
  [_dataSource applySnapshot:snapshot animatingDifferences:NO];
}

/// Sets `_dataSource` and fills the table with data from `_passwordConflicts`.
- (void)initializeDataSourceAndTable {
  /// Set up data source.
  __weak __typeof(self) weakSelf = self;
  UITableViewDiffableDataSourceCellProvider cellProvider = ^UITableViewCell*(
      UITableView* tableView, NSIndexPath* indexPath,
      NSNumber* itemIdentifier) {
    CHECK_EQ(tableView, weakSelf.tableView);
    return [weakSelf cellForIndexPath:indexPath itemIdentifier:itemIdentifier];
  };
  _dataSource =
      [[UITableViewDiffableDataSource alloc] initWithTableView:self.tableView
                                                  cellProvider:cellProvider];
  /// Initialize table.
  NSDiffableDataSourceSnapshot* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];
  [snapshot
      appendSectionsWithIdentifiers:@[ kDataImportInvalidPasswordSection ]];
  NSMutableArray* indicesForPasswordConflicts = [NSMutableArray array];
  for (NSUInteger i = 0; i < _invalidPasswords.count; i++) {
    [indicesForPasswordConflicts addObject:@(i)];
  }
  [snapshot appendItemsWithIdentifiers:indicesForPasswordConflicts
             intoSectionWithIdentifier:kDataImportInvalidPasswordSection];
  [_dataSource applySnapshot:snapshot animatingDifferences:NO];
}

@end
