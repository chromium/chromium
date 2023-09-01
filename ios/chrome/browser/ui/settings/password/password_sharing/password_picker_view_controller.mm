// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/password_picker_view_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/password_ui_utils.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierCredentials = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeCredential = kItemTypeEnumZero,
};

// Size of the accessory view symbol.
const CGFloat kAccessorySymbolSize = 22;

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
  self.navigationItem.title =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_SHARING_TITLE);
  UIBarButtonItem* nextButton = [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(
                        IDS_IOS_PASSWORD_SHARING_PASSWORD_PICKER_NEXT_BUTTON)
              style:UIBarButtonItemStylePlain
             target:self
             action:@selector(nextButtonTapped)];
  nextButton.enabled = NO;
  self.navigationItem.rightBarButtonItem = nextButton;

  self.tableView.allowsMultipleSelection = YES;

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

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [tableView cellForRowAtIndexPath:indexPath].accessoryView =
      [[UIImageView alloc] initWithImage:[self checkmarkCircleIcon]];
  [self setNextButtonStatus];
}

- (void)tableView:(UITableView*)tableView
    didDeselectRowAtIndexPath:(NSIndexPath*)indexPath {
  [tableView cellForRowAtIndexPath:indexPath].accessoryView =
      [[UIImageView alloc] initWithImage:[self circleIcon]];
  [self setNextButtonStatus];
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
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  cell.userInteractionEnabled = YES;
  cell.textLabel.numberOfLines = 1;
  cell.detailTextLabel.numberOfLines = 1;
  cell.accessoryView = [[UIImageView alloc]
      initWithImage:cell.isSelected ? [self checkmarkCircleIcon]
                                    : [self circleIcon]];

  return cell;
}

#pragma mark - PasswordPickerConsumer

- (void)setCredentials:
    (const std::vector<password_manager::CredentialUIEntry>&)credentials {
  _credentials = credentials;

  [self loadModel];
  [self.tableView reloadData];
}

#pragma mark - Items

- (SettingsImageDetailTextItem*)credentialItem:
    (const password_manager::CredentialUIEntry&)credential {
  SettingsImageDetailTextItem* item =
      [[SettingsImageDetailTextItem alloc] initWithType:ItemTypeCredential];
  item.text = base::SysUTF16ToNSString(credential.username);
  // TODO(crbug.com/1463882): Double check which origin should be displayed. For
  // now first is returned.
  item.detailText = base::SysUTF8ToNSString(
      password_manager::GetShownOrigin(credential.facets[0]));
  // TODO(crbug.com/1463882): Add favicon.
  return item;
}

#pragma mark - Private

- (UIImage*)checkmarkCircleIcon {
  return DefaultSymbolWithPointSize(kCheckmarkCircleFillSymbol,
                                    kAccessorySymbolSize);
}

- (UIImage*)circleIcon {
  return DefaultSymbolWithPointSize(kCircleSymbol, kAccessorySymbolSize);
}

- (void)cancelButtonTapped {
  // TODO(crbug.com/1463882): Handle cancel tap.
}

- (void)nextButtonTapped {
  // TODO(crbug.com/1463882): Handle next tap.
}

// Enables next button if any row is selected or disables it otherwise.
- (void)setNextButtonStatus {
  self.navigationItem.rightBarButtonItem.enabled =
      self.tableView.indexPathsForSelectedRows.count > 0;
}

@end
