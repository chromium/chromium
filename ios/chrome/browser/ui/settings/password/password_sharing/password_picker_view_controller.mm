// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/password_picker_view_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/password_ui_utils.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_favicon_data_source.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_picker_view_controller_presentation_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_constants.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Alpha for the disabled passkey cell.
const CGFloat kBackgroundDisabledAlpha = 0.4;

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierCredentials = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeCredential = kItemTypeEnumZero,
};

// Orders passwords before passkeys. For the same type of credentials defaults
// to the existing comparator.
bool CompareCredentialsByType(const password_manager::CredentialUIEntry& lhs,
                              const password_manager::CredentialUIEntry& rhs) {
  bool is_lhs_passkey = !lhs.passkey_credential_id.empty();
  bool is_rhs_passkey = !rhs.passkey_credential_id.empty();
  if (is_lhs_passkey != is_rhs_passkey) {
    return is_lhs_passkey < is_rhs_passkey;
  }
  return lhs < rhs;
}

}  // namespace

@interface PasswordPickerViewController () {
  std::vector<password_manager::CredentialUIEntry> _credentials;
}

@end

@implementation PasswordPickerViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.navigationItem.leftBarButtonItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(cancelButtonTapped)];
  self.navigationItem.leftBarButtonItem.accessibilityIdentifier =
      kPasswordPickerCancelButtonID;
  self.navigationItem.title =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_SHARING_TITLE);
  UIBarButtonItem* nextButton = [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(
                        IDS_IOS_PASSWORD_SHARING_PASSWORD_PICKER_NEXT_BUTTON)
              style:UIBarButtonItemStylePlain
             target:self
             action:@selector(nextButtonTapped)];
  self.navigationItem.rightBarButtonItem = nextButton;
  self.navigationItem.rightBarButtonItem.accessibilityIdentifier =
      kPasswordPickerNextButtonID;
  self.view.accessibilityIdentifier = kPasswordPickerViewID;
  self.clearsSelectionOnViewWillAppear = NO;

  [self loadModel];
}

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierCredentials];
  for (const password_manager::CredentialUIEntry& credential : _credentials) {
    [model addItem:[self credentialItem:credential]
        toSectionWithIdentifier:SectionIdentifierCredentials];
  }
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];

  // Select first row if there is no selection.
  if (!self.tableView.indexPathForSelectedRow) {
    NSIndexPath* indexPath = [NSIndexPath indexPathForRow:0 inSection:0];
    [self.tableView selectRowAtIndexPath:indexPath
                                animated:NO
                          scrollPosition:UITableViewScrollPositionNone];
  }
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [tableView cellForRowAtIndexPath:indexPath].accessoryType =
      UITableViewCellAccessoryCheckmark;
}

- (void)tableView:(UITableView*)tableView
    didDeselectRowAtIndexPath:(NSIndexPath*)indexPath {
  [tableView cellForRowAtIndexPath:indexPath].accessoryType =
      UITableViewCellAccessoryNone;
}

#pragma mark - UITableViewDataSource

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  return _credentials.size();
}

- (NSInteger)numberOfSectionsInTableView:(UITableView*)theTableView {
  return 1;
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  BOOL isPassword = _credentials[indexPath.row].passkey_credential_id.empty();
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];

  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  cell.userInteractionEnabled = isPassword;
  cell.textLabel.numberOfLines = 1;
  cell.detailTextLabel.numberOfLines = 1;
  if (indexPath.row == tableView.indexPathForSelectedRow.row) {
    cell.accessoryType = UITableViewCellAccessoryCheckmark;
  } else {
    cell.accessoryType = UITableViewCellAccessoryNone;
  }
  if (!isPassword) {
    cell.contentView.alpha = kBackgroundDisabledAlpha;
  }

  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  TableViewURLItem* URLItem =
      base::apple::ObjCCastStrict<TableViewURLItem>(item);
  TableViewURLCell* URLCell =
      base::apple::ObjCCastStrict<TableViewURLCell>(cell);
  [self.imageDataSource
      faviconForPageURL:URLItem.URL
             completion:^(FaviconAttributes* attributes) {
               [URLCell.faviconView configureWithAttributes:attributes];
             }];

  return cell;
}

#pragma mark - PasswordPickerConsumer

- (void)setCredentials:
    (const std::vector<password_manager::CredentialUIEntry>&)credentials {
  _credentials = credentials;

  // Ensure that passkeys are at the end since they cannot be shared currently.
  std::sort(_credentials.begin(), _credentials.end(), CompareCredentialsByType);

  [self loadModel];
  [self.tableView reloadData];
}

#pragma mark - Items

- (TableViewURLItem*)credentialItem:
    (const password_manager::CredentialUIEntry&)credential {
  TableViewURLItem* item =
      [[TableViewURLItem alloc] initWithType:ItemTypeCredential];
  item.title = base::SysUTF16ToNSString(credential.username);
  item.URL = [[CrURL alloc] initWithGURL:GURL(credential.GetURL())];
  if (!credential.passkey_credential_id.empty()) {
    item.detailText = l10n_util::GetNSString(
        IDS_IOS_PASSWORD_SHARING_PASSWORD_PICKER_PASSKEY_INFO);
  }
  return item;
}

#pragma mark - Private

- (void)cancelButtonTapped {
  [self.delegate passwordPickerWasDismissed:self];
}

- (void)nextButtonTapped {
  [self.delegate
        passwordPickerClosed:self
      withSelectedCredential:_credentials[self.tableView.indexPathForSelectedRow
                                              .row]];
}

@end
