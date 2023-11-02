// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/new_password_view_controller.h"

#import "base/notreached.h"
#import "ios/chrome/common/app_group/app_group_metrics.h"
#import "ios/chrome/common/credential_provider/archivable_credential.h"
#import "ios/chrome/common/credential_provider/constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/form_input_accessory_view.h"
#import "ios/chrome/common/ui/elements/form_input_accessory_view_text_data.h"
#import "ios/chrome/credential_provider_extension/metrics_util.h"
#import "ios/chrome/credential_provider_extension/ui/new_password_footer_view.h"
#import "ios/chrome/credential_provider_extension/ui/new_password_table_cell.h"
#import "ios/chrome/credential_provider_extension/ui/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Desired space between the bottom of the nav bar and the top of the table
// view.
const CGFloat kTableViewTopSpace = 14;

}  // namespace

@interface NewPasswordViewController () <FormInputAccessoryViewDelegate,
                                         NewPasswordTableCellDelegate,
                                         UITableViewDataSource>

// The current creation type of the entered password.
@property(nonatomic, assign) CPEPasswordCreated passwordCreationType;

// Input accessory view for the text fields
@property(nonatomic, strong) FormInputAccessoryView* accessoryView;

// The cell for username entry.
@property(nonatomic, readonly) NewPasswordTableCell* usernameCell;

// The cell for password entry
@property(nonatomic, readonly) NewPasswordTableCell* passwordCell;

@end

@implementation NewPasswordViewController

- (instancetype)init {
  UITableViewStyle style = UITableViewStyleInsetGrouped;
  self = [super initWithStyle:style];
  _passwordCreationType = CPEPasswordCreated::kPasswordManuallyEntered;
  _accessoryView = [[FormInputAccessoryView alloc] init];
  [_accessoryView setUpWithLeadingView:nil navigationDelegate:self];
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  UIColor* backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
  self.view.backgroundColor = backgroundColor;

  UINavigationBarAppearance* appearance =
      [[UINavigationBarAppearance alloc] init];
  [appearance configureWithDefaultBackground];
  appearance.backgroundColor = backgroundColor;
  if (@available(iOS 15, *)) {
    self.navigationItem.scrollEdgeAppearance = appearance;
  } else {
    // On iOS 14, scrollEdgeAppearance only affects navigation bars with large
    // titles, so it can't be used. Instead, the navigation bar will always be
    // the same style.
    self.navigationItem.standardAppearance = appearance;
  }

  self.title =
      NSLocalizedString(@"IDS_IOS_CREDENTIAL_PROVIDER_NEW_PASSWORD_TITLE",
                        @"Title for add new password screen");
  self.navigationItem.leftBarButtonItem = [self navigationCancelButton];
  self.navigationItem.rightBarButtonItem = [self navigationSaveButton];

  // UITableViewStyleInsetGrouped adds space to the top of the table view by
  // default. Remove that space and add in the desired amount.
  self.tableView.contentInset = UIEdgeInsetsMake(
      -kUITableViewInsetGroupedTopSpace + kTableViewTopSpace, 0, 0, 0);

  [self.tableView registerClass:[NewPasswordTableCell class]
         forCellReuseIdentifier:NewPasswordTableCell.reuseID];
  [self.tableView registerClass:[NewPasswordFooterView class]
      forHeaderFooterViewReuseIdentifier:NewPasswordFooterView.reuseID];
}

#pragma mark - UITableViewDataSource

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  // If password sync is not on (represented by the user's email not being
  // available as used in the sync disclaimer), then don't show the "Suggest
  // Strong Password" button.
  NSString* syncingUserEmail = [app_group::GetGroupUserDefaults()
      stringForKey:AppGroupUserDefaultsCredentialProviderUserEmail()];
  BOOL passwordSyncOn = syncingUserEmail != nil;
  return (passwordSyncOn) ? NewPasswordTableCellTypeNumRows
                          : NewPasswordTableCellTypeNumRows - 1;
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  NewPasswordTableCell* cell = [tableView
      dequeueReusableCellWithIdentifier:NewPasswordTableCell.reuseID];
  cell.textField.inputAccessoryView = self.accessoryView;

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

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  return [tableView
      dequeueReusableHeaderFooterViewWithIdentifier:NewPasswordFooterView
                                                        .reuseID];
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
  // other cells are unselectable from `-tableView:willSelectRowAtIndexPath:`.
  [self.credentialHandler userDidRequestGeneratedPassword];
  self.passwordCreationType = CPEPasswordCreated::kPasswordSuggested;
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

- (NewPasswordTableCell*)usernameCell {
  NSIndexPath* usernameIndexPath =
      [NSIndexPath indexPathForRow:NewPasswordTableCellTypeUsername
                         inSection:0];
  NewPasswordTableCell* usernameCell =
      [self.tableView cellForRowAtIndexPath:usernameIndexPath];

  return usernameCell;
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

- (void)textFieldDidBeginEditingInCell:(NewPasswordTableCell*)cell {
  self.accessoryView.previousButton.enabled = (cell == self.passwordCell);
  self.accessoryView.nextButton.enabled = (cell == self.usernameCell);
}

- (void)textFieldDidChangeInCell:(NewPasswordTableCell*)cell {
  if (cell == self.passwordCell) {
    // Update the password creation type so the correct histogram value can be
    // fired when the password is actually created.
    if (self.passwordCreationType == CPEPasswordCreated::kPasswordSuggested) {
      self.passwordCreationType =
          CPEPasswordCreated::kPasswordSuggestedAndChanged;
    } else if (self.passwordCell.textField.text.length == 0) {
      // When the password field is empty, reset the creation type to manual as
      // any traces of the suggested password are now gone.
      self.passwordCreationType = CPEPasswordCreated::kPasswordManuallyEntered;
    }
    [self updateSaveButtonState];
  }
}

- (BOOL)textFieldShouldReturnInCell:(NewPasswordTableCell*)cell {
  if (cell == self.usernameCell) {
    [self.passwordCell.textField becomeFirstResponder];
  } else if (cell == self.passwordCell) {
    [self.passwordCell.textField resignFirstResponder];
  }
  return NO;
}

// Updates the save button state based on whether there is text in the password
// cell.
- (void)updateSaveButtonState {
  self.navigationItem.rightBarButtonItem.enabled =
      self.passwordCell.textField.text.length > 0;
}

#pragma mark - Private

// Action for cancel button.
- (void)cancelButtonWasPressed {
  [self.delegate
      navigationCancelButtonWasPressedInNewPasswordViewController:self];
}

// Action for save button.
- (void)saveButtonWasPressed {
  [self saveCredential:NO];
}

- (NSString*)currentUsername {
  NSIndexPath* usernameIndexPath =
      [NSIndexPath indexPathForRow:NewPasswordTableCellTypeUsername
                         inSection:0];
  NewPasswordTableCell* usernameCell =
      [self.tableView cellForRowAtIndexPath:usernameIndexPath];
  return usernameCell.textField.text;
}

- (NSString*)currentPassword {
  NSIndexPath* passwordIndexPath =
      [NSIndexPath indexPathForRow:NewPasswordTableCellTypePassword
                         inSection:0];
  NewPasswordTableCell* passwordCell =
      [self.tableView cellForRowAtIndexPath:passwordIndexPath];
  return passwordCell.textField.text;
}

// Saves the current data as a credential. If `shouldReplace` is YES, then the
// user has already said they are aware that they are replacing a previous
// credential.
- (void)saveCredential:(BOOL)shouldReplace {
  NSString* username = [self currentUsername];
  NSString* password = [self currentPassword];

  [self.credentialHandler saveCredentialWithUsername:username
                                            password:password
                                       shouldReplace:shouldReplace];
}

#pragma mark - NewPasswordUIHandler

- (void)setPassword:(NSString*)password {
  NewPasswordTableCell* passwordCell = self.passwordCell;
  passwordCell.textField.text = password;
  // Move voiceover focus to the save button so the user knows that something
  // has happend and the save button is now enabled.
  UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,
                                  self.navigationItem.rightBarButtonItem);
  [self updateSaveButtonState];
}

// Alerts the user that saving their password failed.
- (void)alertSavePasswordFailed {
  UIAlertController* alertController = [UIAlertController
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
  [alertController addAction:defaultAction];
  [self presentViewController:alertController animated:YES completion:nil];
}

- (void)alertUserCredentialExists {
  NSString* messageBaseLocalizedString = NSLocalizedString(
      @"IDS_IOS_CREDENTIAL_PROVIDER_NEW_PASSWORD_REPLACE_MESSAGE",
      @"Message for password replace alert");
  NSString* message = [[messageBaseLocalizedString
      stringByReplacingOccurrencesOfString:@"$2"
                                withString:self.currentHost]
      stringByReplacingOccurrencesOfString:@"$1"
                                withString:[self currentUsername]];
  UIAlertController* alertController = [UIAlertController
      alertControllerWithTitle:
          NSLocalizedString(
              @"IDS_IOS_CREDENTIAL_PROVIDER_NEW_PASSWORD_REPLACE_TITLE",
              @"Replace password?")
                       message:message
                preferredStyle:UIAlertControllerStyleAlert];
  __weak __typeof(self) weakSelf = self;
  UIAlertAction* replaceAction = [UIAlertAction
      actionWithTitle:NSLocalizedString(
                          @"IDS_IOS_CREDENTIAL_PROVIDER_EXTENSION_REPLACE",
                          @"Replace")
                style:UIAlertActionStyleDestructive
              handler:^(UIAlertAction* action) {
                [weakSelf saveCredential:YES];
              }];
  [alertController addAction:replaceAction];
  UIAlertAction* cancelAction = [UIAlertAction
      actionWithTitle:NSLocalizedString(
                          @"IDS_IOS_CREDENTIAL_PROVIDER_EXTENSION_CANCEL",
                          @"Cancel")
                style:UIAlertActionStyleCancel
              handler:^(UIAlertAction* action){
              }];
  [alertController addAction:cancelAction];
  [self presentViewController:alertController animated:YES completion:nil];
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
  saveButton.style = UIBarButtonItemStyleDone;

  // Save button should start disabled because no password has been entered.
  saveButton.enabled = NO;
  return saveButton;
}

- (void)credentialSaved:(ArchivableCredential*)credential {
  CPENewCredentialUsername usernameType =
      (credential.user.length)
          ? CPENewCredentialUsername::kCredentialWithUsername
          : CPENewCredentialUsername::kCredentialWithoutUsername;
  UpdateHistogramCount(@"IOS.CredentialExtension.NewCredentialUsername",
                       static_cast<int>(usernameType));
  UpdateHistogramCount(@"IOS.CredentialExtension.PasswordCreated",
                       static_cast<int>(self.passwordCreationType));
}

#pragma mark - FormInputAccessoryViewDelegate

- (void)formInputAccessoryViewDidTapNextButton:(FormInputAccessoryView*)sender {
  // The next button should only be enabled in the username field, going to the
  // password field.
  [self.passwordCell.textField becomeFirstResponder];
}

- (void)formInputAccessoryViewDidTapPreviousButton:
    (FormInputAccessoryView*)sender {
  // The previous button should only be enabled in the password field, going
  // back to the username field.
  [self.usernameCell.textField becomeFirstResponder];
}

- (void)formInputAccessoryViewDidTapCloseButton:
    (FormInputAccessoryView*)sender {
  [self.view endEditing:YES];
}

- (FormInputAccessoryViewTextData*)textDataforFormInputAccessoryView:
    (FormInputAccessoryView*)sender {
  return [[FormInputAccessoryViewTextData alloc]
              initWithCloseButtonTitle:NSLocalizedString(
                                           @"IDS_IOS_CREDENTIAL_PROVIDER_DONE",
                                           @"Done")
         closeButtonAccessibilityLabel:
             NSLocalizedString(
                 @"IDS_IOS_CREDENTIAL_PROVIDER_NEW_PASSWORD_HIDE_KEYBOARD_HINT",
                 @"Hide Keyboard")
          nextButtonAccessibilityLabel:
              NSLocalizedString(
                  @"IDS_IOS_CREDENTIAL_PROVIDER_NEW_PASSWORD_NEXT_FIELD_HINT",
                  @"Next field")
      previousButtonAccessibilityLabel:
          NSLocalizedString(
              @"IDS_IOS_CREDENTIAL_PROVIDER_NEW_PASSWORD_PREVIOUS_FIELD_HINT",
              @"Previous field")];
}

@end
