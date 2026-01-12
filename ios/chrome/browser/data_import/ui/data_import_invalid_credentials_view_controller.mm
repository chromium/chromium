// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/data_import/ui/data_import_invalid_credentials_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/check_op.h"
#import "ios/chrome/browser/data_import/public/accessibility_utils.h"
#import "ios/chrome/browser/data_import/public/credential_item_identifier.h"
#import "ios/chrome/browser/data_import/public/passkey_import_item.h"
#import "ios/chrome/browser/data_import/public/password_import_item.h"
#import "ios/chrome/browser/data_import/ui/credential_import_item_cell_content_configuration.h"
#import "ios/chrome/browser/data_import/ui/ui_utils.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_attributed_string_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
/// The identifier for the only section in the table.
NSString* const kDataImportInvalidCredentialsSection =
    @"kDataImportInvalidCredentialsSection";
}  // namespace

@interface DataImportInvalidCredentialsViewController () <UITableViewDelegate>
@end

@implementation DataImportInvalidCredentialsViewController {
  /// List of invalid credentials.
  NSArray<CredentialImportItem*>* _invalidCredentials;
  /// Type of invalid credentials displayed in this view.
  CredentialType _credentialType;
  /// The data source painting each cell in the table from
  /// `_invalidCredentials`.
  UITableViewDiffableDataSource<NSString*, CredentialItemIdentifier*>*
      _dataSource;
}

#pragma mark - ChromeTableViewController

- (instancetype)initWithInvalidCredentials:
                    (NSArray<CredentialImportItem*>*)credentials
                                      type:(CredentialType)type {
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _invalidCredentials = credentials;
    _credentialType = type;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title =
      l10n_util::GetNSString(IDS_IOS_SAFARI_IMPORT_INVALID_PASSWORD_LIST_TITLE);
  if (@available(iOS 26, *)) {
    self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc]
        initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                             target:self
                             action:@selector(didTapCloseButton)];
  } else {
    NSString* closeButtonTitle = l10n_util::GetNSString(
        IDS_IOS_SAFARI_IMPORT_INVALID_PASSWORD_LIST_BUTTON_CLOSE);
    UIBarButtonItem* closeButton =
        [[UIBarButtonItem alloc] initWithTitle:closeButtonTitle
                                         style:UIBarButtonItemStylePlain
                                        target:self
                                        action:@selector(didTapCloseButton)];
    self.navigationItem.rightBarButtonItem = closeButton;
  }
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
  int messageId = _credentialType == CredentialType::kPassword
                      ? IDS_IOS_IMPORT_INVALID_PASSWORD_LIST_DESCRIPTION
                      : IDS_IOS_IMPORT_INVALID_PASSKEY_LIST_DESCRIPTION;
  NSMutableAttributedString* attributedText = [[NSMutableAttributedString alloc]
      initWithString:l10n_util::GetNSString(messageId)
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
                      itemIdentifier:(CredentialItemIdentifier*)identifier {
  CredentialImportItem* item = _invalidCredentials[identifier.index];
  CredentialImportItemCellContentConfiguration* config;
  if (_credentialType == CredentialType::kPassword) {
    config = [CredentialImportItemCellContentConfiguration
        cellConfigurationForErrorMessage:base::apple::ObjCCastStrict<
                                             PasswordImportItem>(item)];
  } else {
    config = [CredentialImportItemCellContentConfiguration
        cellConfigurationForPasskey:base::apple::ObjCCastStrict<
                                        PasskeyImportItem>(item)];
  }
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
- (void)updateItemWithIdentifier:(CredentialItemIdentifier*)identifier {
  NSDiffableDataSourceSnapshot<NSString*, CredentialItemIdentifier*>* snapshot =
      [_dataSource snapshot];
  [snapshot reconfigureItemsWithIdentifiers:@[ identifier ]];
  [_dataSource applySnapshot:snapshot animatingDifferences:NO];
}

/// Sets `_dataSource` and fills the table with data from `_invalidCredentials`.
- (void)initializeDataSourceAndTable {
  /// Set up data source.
  __weak __typeof(self) weakSelf = self;
  UITableViewDiffableDataSourceCellProvider cellProvider = ^UITableViewCell*(
      UITableView* tableView, NSIndexPath* indexPath,
      CredentialItemIdentifier* itemIdentifier) {
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
      appendSectionsWithIdentifiers:@[ kDataImportInvalidCredentialsSection ]];
  NSMutableArray* indicesForInvalidCredentials = [NSMutableArray array];
  for (NSUInteger i = 0; i < _invalidCredentials.count; i++) {
    [indicesForInvalidCredentials
        addObject:[[CredentialItemIdentifier alloc] initWithType:_credentialType
                                                           index:i]];
  }
  [snapshot appendItemsWithIdentifiers:indicesForInvalidCredentials
             intoSectionWithIdentifier:kDataImportInvalidCredentialsSection];
  [_dataSource applySnapshot:snapshot animatingDifferences:NO];
}

@end
