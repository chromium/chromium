// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/ui/safari_data_invalid_passwords_view_controller.h"

#import "base/check_op.h"
#import "base/notreached.h"
#import "ios/chrome/browser/safari_data_import/public/password_import_item.h"
#import "ios/chrome/browser/safari_data_import/public/utils.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_attributed_string_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

/// The identifier for the only section in the table.
NSString* const kSafariDataInvalidPasswordSection =
    @"kSafariDataInvalidPasswordSection";

/// Returns the error message corresponding to the password import status.
NSString* GetErrorMessageForPasswordImportStatus(PasswordImportStatus status) {
  int message_id;
  switch (status) {
    case PasswordImportStatus::kUnknownError:
      message_id = IDS_IOS_SAFARI_IMPORT_INVALID_PASSWORD_REASON_UNKNOWN;
      break;
    case PasswordImportStatus::kMissingPassword:
      message_id =
          IDS_IOS_SAFARI_IMPORT_INVALID_PASSWORD_REASON_MISSING_PASSWORD;
      break;
    case PasswordImportStatus::kMissingURL:
      message_id = IDS_IOS_SAFARI_IMPORT_INVALID_PASSWORD_REASON_MISSING_URL;
      break;
    case PasswordImportStatus::kInvalidURL:
      message_id = IDS_IOS_SAFARI_IMPORT_INVALID_PASSWORD_REASON_INVALID_URL;
      break;
    case PasswordImportStatus::kLongUrl:
      message_id = IDS_IOS_SAFARI_IMPORT_INVALID_PASSWORD_REASON_LONG_URL;
      break;
    case PasswordImportStatus::kLongPassword:
      message_id = IDS_IOS_SAFARI_IMPORT_INVALID_PASSWORD_REASON_LONG_PASSWORD;
      break;
    case PasswordImportStatus::kLongUsername:
      message_id = IDS_IOS_SAFARI_IMPORT_INVALID_PASSWORD_REASON_LONG_USERNAME;
      break;
    case PasswordImportStatus::kLongNote:
    case PasswordImportStatus::kLongConcatenatedNote:
      message_id = IDS_IOS_SAFARI_IMPORT_INVALID_PASSWORD_REASON_LONG_NOTE;
      break;
    case PasswordImportStatus::kConflictProfile:
    case PasswordImportStatus::kConflictAccount:
    case PasswordImportStatus::kNone:
    case PasswordImportStatus::kValid:
      NOTREACHED();
  }
  return l10n_util::GetNSString(message_id);
}

}  // namespace

@interface SafariDataInvalidPasswordsViewController () <UITableViewDelegate>

@end

@implementation SafariDataInvalidPasswordsViewController {
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
  /// Register cells.
  RegisterTableViewCell<TableViewURLCell>(self.tableView);
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
- (TableViewURLCell*)cellForIndexPath:(NSIndexPath*)indexPath
                       itemIdentifier:(NSNumber*)identifier {
  TableViewURLCell* cell =
      DequeueTableViewCell<TableViewURLCell>(self.tableView);
  cell.accessibilityIdentifier =
      GetInvalidPasswordsTableViewCellAccessibilityIdentifier(indexPath.item);
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  /// Populate cell with information.
  PasswordImportItem* item = _invalidPasswords[identifier.intValue];
  cell.titleLabel.text = item.url;
  cell.titleLabel.numberOfLines = 1;
  cell.titleLabel.lineBreakMode = NSLineBreakByTruncatingTail;
  cell.URLLabel.text = item.username;
  cell.thirdRowLabel.text = GetErrorMessageForPasswordImportStatus(item.status);
  cell.thirdRowLabel.textColor = [UIColor colorNamed:kRed600Color];
  if (item.faviconAttributes) {
    [cell.faviconView configureWithAttributes:item.faviconAttributes];
  } else {
    __weak __typeof(self) weakSelf = self;
    [item loadFaviconWithCompletionHandler:^{
      [weakSelf updateItemWithIdentifier:identifier];
    }];
  }
  [cell configureUILayout];
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
      appendSectionsWithIdentifiers:@[ kSafariDataInvalidPasswordSection ]];
  NSMutableArray* indicesForPasswordConflicts = [NSMutableArray array];
  for (NSUInteger i = 0; i < _invalidPasswords.count; i++) {
    [indicesForPasswordConflicts addObject:@(i)];
  }
  [snapshot appendItemsWithIdentifiers:indicesForPasswordConflicts
             intoSectionWithIdentifier:kSafariDataInvalidPasswordSection];
  [_dataSource applySnapshot:snapshot animatingDifferences:NO];
}

@end
