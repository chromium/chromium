// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/new_password_view_controller.h"

#include "base/notreached.h"
#include "ios/chrome/common/app_group/app_group_metrics.h"
#import "ios/chrome/common/credential_provider/archivable_credential.h"
#import "ios/chrome/common/credential_provider/constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/credential_provider_extension/metrics_util.h"
#import "ios/chrome/credential_provider_extension/ui/new_password_table_cell.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface NewPasswordViewController () <UITableViewDataSource,
                                         NewPasswordTableCellDelegate>
@end

@implementation NewPasswordViewController

- (instancetype)init {
  UITableViewStyle style = UITableViewStyleInsetGrouped;
  self = [super initWithStyle:style];
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  UIColor* backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
  self.view.backgroundColor = backgroundColor;
  self.navigationController.navigationBar.translucent = NO;
  self.navigationController.navigationBar.backgroundColor = backgroundColor;

  self.title =
      NSLocalizedString(@"IDS_IOS_CREDENTIAL_PROVIDER_NEW_PASSWORD_TITLE",
                        @"Title for add new password screen");
  self.navigationItem.leftBarButtonItem = [self navigationCancelButton];
  self.navigationItem.rightBarButtonItem = [self navigationSaveButton];

  [self.tableView registerClass:[NewPasswordTableCell class]
         forCellReuseIdentifier:NewPasswordTableCell.reuseID];
  self.tableView.rowHeight = UITableViewAutomaticDimension;
  self.tableView.estimatedRowHeight = 44.0;
}

#pragma mark - UITableViewDataSource

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  return NewPasswordTableCellTypeNumRows;
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  NewPasswordTableCell* cell = [tableView
      dequeueReusableCellWithIdentifier:NewPasswordTableCell.reuseID];

  NewPasswordTableCellType cellType;
  switch (indexPath.row) {
    case NewPasswordTableCellTypeUsername:
      cellType = NewPasswordTableCellTypeUsername;
      cell.selectionStyle = UITableViewCellSelectionStyleNone;
      break;
    case NewPasswordTableCellTypePassword:
      cellType = NewPasswordTableCellTypePassword;
      cell.selectionStyle = UITableViewCellSelectionStyleNone;
      break;
    case NewPasswordTableCellTypeSuggestStrongPassword:
      cellType = NewPasswordTableCellTypeSuggestStrongPassword;
      cell.selectionStyle = UITableViewCellSelectionStyleDefault;
      cell.accessibilityTraits |= UIAccessibilityTraitButton;
      break;
    default:
      NOTREACHED();
      cellType = NewPasswordTableCellTypeSuggestStrongPassword;
      break;
  }

  [cell setCellType:cellType];
  cell.delegate = self;

  return cell;
}

- (NSString*)tableView:(UITableView*)tableView
    titleForFooterInSection:(NSInteger)section {
  NSString* userEmail = [app_group::GetGroupUserDefaults()
      stringForKey:AppGroupUserDefaultsCredentialProviderUserEmail()];
  if (userEmail) {
    NSString* baseLocalizedString = NSLocalizedString(
        @"IDS_IOS_CREDENTIAL_PROVIDER_NEW_PASSWORD_FOOTER",
        @"Disclaimer telling users what will happen to their passwords");
    return [baseLocalizedString stringByReplacingOccurrencesOfString:@"$1"
                                                          withString:userEmail];
  } else {
    return NSLocalizedString(
        @"IDS_IOS_CREDENTIAL_PROVIDER_NEW_PASSWORD_FOOTER_NO_EMAIL",
        @"Disclaimer telling non-logged in users what will happen to their "
        @"passwords");
  }
}

#pragma mark - UITableViewDelegate

- (NSIndexPath*)tableView:(UITableView*)tableView
    willSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  switch (indexPath.row) {
    case NewPasswordTableCellTypeUsername:
    case NewPasswordTableCellTypePassword:
      return nil;
    case NewPasswordTableCellTypeSuggestStrongPassword:
      return indexPath;
    default:
      return indexPath;
  }
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  // There is no need to check which cell has been selected because all the
  // other cells are unselectable from |-tableView:willSelectRowAtIndexPath:|.

  NewPasswordTableCell* passwordCell = [self passwordCell];

  // TODO(crbug.com/1224986): Generate password and fill it in.
  passwordCell.textField.text = @"";

  [self updateSaveButtonState];

  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

- (NewPasswordTableCell*)passwordCell {
  NSIndexPath* passwordIndexPath =
      [NSIndexPath indexPathForRow:NewPasswordTableCellTypePassword
                         inSection:0];
  NewPasswordTableCell* passwordCell =
      [self.tableView cellForRowAtIndexPath:passwordIndexPath];

  return passwordCell;
}

#pragma mark - NewPasswordTableCellDelegate

- (void)textFieldDidChangeInCell:(NewPasswordTableCell*)cell {
  if (cell == [self passwordCell]) {
    [self updateSaveButtonState];
  }
}

// Updates the save button state based on whether there is text in the password
// cell.
- (void)updateSaveButtonState {
  self.navigationItem.rightBarButtonItem.enabled =
      [self passwordCell].textField.text.length > 0;
}

#pragma mark - Private

// Action for cancel button.
- (void)cancelButtonWasPressed {
  [self.delegate
      navigationCancelButtonWasPressedInNewPasswordViewController:self];
}

// Action for save button.
- (void)saveButtonWasPressed {
  NSIndexPath* usernameIndexPath =
      [NSIndexPath indexPathForRow:NewPasswordTableCellTypeUsername
                         inSection:0];
  NewPasswordTableCell* usernameCell =
      [self.tableView cellForRowAtIndexPath:usernameIndexPath];
  NSString* username = usernameCell.textField.text;

  NSIndexPath* passwordIndexPath =
      [NSIndexPath indexPathForRow:NewPasswordTableCellTypePassword
                         inSection:0];
  NewPasswordTableCell* passwordCell =
      [self.tableView cellForRowAtIndexPath:passwordIndexPath];
  NSString* password = passwordCell.textField.text;

  ArchivableCredential* credential =
      [self.credentialHandler createNewCredentialWithUsername:username
                                                     password:password];

  if (!credential) {
    [self savePasswordFailed];
    return;
  }

  [self.credentialHandler
      saveNewCredential:credential
             completion:^(NSError* error) {
               if (error) {
                 UpdateUMACountForKey(
                     app_group::kCredentialExtensionSaveCredentialFailureCount);
                 [self savePasswordFailed];
                 return;
               }
               [self.delegate userSelectedCredential:credential];
             }];
}

// Alerts the user that saving their password failed.
- (void)savePasswordFailed {
  UIAlertController* ac = [UIAlertController
      alertControllerWithTitle:
          NSLocalizedString(
              @"IDS_IOS_CREDENTIAL_PROVIDER_NEW_PASSWORD_ERROR_TITLE",
              @"Title for password save error")
                       message:NSLocalizedString(
                                   @"IDS_IOS_CREDENTIAL_PROVIDER_NEW_PASSWORD_"
                                   @"ERROR_MESSAGE",
                                   @"Message for password save error")
                preferredStyle:UIAlertControllerStyleAlert];
  UIAlertAction* defaultAction = [UIAlertAction
      actionWithTitle:NSLocalizedString(@"IDS_IOS_CREDENTIAL_PROVIDER_OK",
                                        @"OK")
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction* action){
              }];
  [ac addAction:defaultAction];
  [self presentViewController:ac animated:YES completion:nil];
}

// Returns a new cancel button for the navigation bar.
- (UIBarButtonItem*)navigationCancelButton {
  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(cancelButtonWasPressed)];
  cancelButton.tintColor = [UIColor colorNamed:kBlueColor];
  return cancelButton;
}

// Returns a new save button for the navigation bar.
- (UIBarButtonItem*)navigationSaveButton {
  UIBarButtonItem* saveButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemSave
                           target:self
                           action:@selector(saveButtonWasPressed)];
  saveButton.tintColor = [UIColor colorNamed:kBlueColor];

  // Save button should start disabled because no password has been entered.
  saveButton.enabled = NO;
  return saveButton;
}

@end
