// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/new_password_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/notreached.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/common/password_manager_constants.h"
#import "ios/chrome/common/app_group/app_group_metrics.h"
#import "ios/chrome/common/credential_provider/archivable_credential.h"
#import "ios/chrome/common/credential_provider/constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/form_input_accessory_view.h"
#import "ios/chrome/common/ui/elements/form_input_accessory_view_text_data.h"
#import "ios/chrome/credential_provider_extension/metrics_util.h"
#import "ios/chrome/credential_provider_extension/ui/new_password_footer_view.h"
#import "ios/chrome/credential_provider_extension/ui/new_password_table_cell.h"
#import "ios/chrome/credential_provider_extension/ui/password_note_cell.h"
#import "ios/chrome/credential_provider_extension/ui/password_note_footer_view.h"
#import "ios/chrome/credential_provider_extension/ui/ui_util.h"

namespace {

// Desired space between the bottom of the nav bar and the top of the table
// view.
const CGFloat kTableViewTopSpace = 14;

// Minimal amount of characters in password note to display the warning.
const int kMinNoteCharAmountForWarning = 901;

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierPassword,
  SectionIdentifierNote,
  SectionIdentifierNumSections
};

}  // namespace

@interface NewPasswordViewController () <FormInputAccessoryViewDelegate,
                                         NewPasswordTableCellDelegate,
                                         PasswordNoteCellDelegate,
                                         UITableViewDataSource>

// The current creation type of the entered password.
@property(nonatomic, assign) CPEPasswordCreated passwordCreationType;

// Input accessory view for the text fields
@property(nonatomic, strong) FormInputAccessoryView* accessoryView;

// The cell for username entry.
@property(nonatomic, readonly) NewPasswordTableCell* usernameCell;

// The cell for password entry
@property(nonatomic, readonly) NewPasswordTableCell* passwordCell;

// The cell for note entry.
@property(nonatomic, readonly) PasswordNoteCell* noteCell;

// The value of the username text.
@property(nonatomic, strong) NSString* usernameText;

// The value of the password text.
@property(nonatomic, strong) NSString* passwordText;

// The value of the note text.
@property(nonatomic, strong) NSString* noteText;

// If yes, the footer informing about the max note length is shown.
@property(nonatomic, assign) BOOL isNoteFooterShown;

@end

@implementation NewPasswordViewController

- (instancetype)init {
  UITableViewStyle style = UITableViewStyleInsetGrouped;
  self = [super initWithStyle:style];
  _passwordCreationType = CPEPasswordCreated::kPasswordManuallyEntered;
  _accessoryView = [[FormInputAccessoryView alloc] init];
  [_accessoryView setUpWithLeadingView:nil navigationDelegate:self];
  _usernameText = @"";
  _passwordText = @"";
  _noteText = @"";
  _isNoteFooterShown = NO;
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
  self.navigationItem.scrollEdgeAppearance = appearance;
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
  [self.tableView registerClass:[PasswordNoteCell class]
         forCellReuseIdentifier:PasswordNoteCell.reuseID];
  [self.tableView registerClass:[PasswordNoteFooterView class]
      forHeaderFooterViewReuseIdentifier:PasswordNoteFooterView.reuseID];
}

#pragma mark - UITableViewDataSource

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  if (section == SectionIdentifierNote) {
    return 1;
  }

  // If password sync is not on (represented by the user's email not being
  // available as used in the sync disclaimer), then don't show the "Suggest
  // Strong Password" button.
  NSString* syncingUserEmail = [app_group::GetGroupUserDefaults()
      stringForKey:AppGroupUserDefaultsCredentialProviderUserEmail()];
  BOOL passwordSyncOn = syncingUserEmail != nil;
  return (passwordSyncOn) ? NewPasswordTableCellTypeNumRows
                          : NewPasswordTableCellTypeNumRows - 1;
}

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView {
  return SectionIdentifierNumSections;
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  if (indexPath.section == SectionIdentifierNote) {
    DCHECK(indexPath.row == 0);
    PasswordNoteCell* cell =
        [tableView dequeueReusableCellWithIdentifier:PasswordNoteCell.reuseID];
    [cell configureCell];
    cell.delegate = self;
    cell.selectionStyle = UITableViewCellSelectionStyleNone;
    return cell;
  }

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
      NOTREACHED_IN_MIGRATION();
      cellType = NewPasswordTableCellTypeSuggestStrongPassword;
      break;
  }

  [cell setCellType:cellType];
  cell.delegate = self;

  return cell;
}

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  if (section == SectionIdentifierPassword) {
    return [tableView
        dequeueReusableHeaderFooterViewWithIdentifier:NewPasswordFooterView
                                                          .reuseID];
  }
  if (section == SectionIdentifierNote) {
    return [tableView
        dequeueReusableHeaderFooterViewWithIdentifier:PasswordNoteFooterView
                                                          .reuseID];
  }

  return nil;
}

#pragma mark - UITableViewDelegate

// Makes sure that the note footer is displayed correctly when it is scrolled to
// as it could be updated when it is not visible on screen with a long note.
- (void)tableView:(UITableView*)tableView
    willDisplayFooterView:(UIView*)view
               forSection:(NSInteger)section {
  if (section == SectionIdentifierNote &&
      [view isKindOfClass:[PasswordNoteFooterView class]]) {
    PasswordNoteFooterView* footer =
        base::apple::ObjCCastStrict<PasswordNoteFooterView>(view);
    footer.textLabel.text = [self noteFooterText];

    [tableView beginUpdates];
    [tableView endUpdates];
  }
}

- (NSIndexPath*)tableView:(UITableView*)tableView
    willSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  if (indexPath.section == SectionIdentifierNote) {
    return nil;
  }

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
                         inSection:SectionIdentifierPassword];
  NewPasswordTableCell* usernameCell =
      [self.tableView cellForRowAtIndexPath:usernameIndexPath];

  return usernameCell;
}

- (NewPasswordTableCell*)passwordCell {
  NSIndexPath* passwordIndexPath =
      [NSIndexPath indexPathForRow:NewPasswordTableCellTypePassword
                         inSection:SectionIdentifierPassword];
  NewPasswordTableCell* passwordCell =
      [self.tableView cellForRowAtIndexPath:passwordIndexPath];

  return passwordCell;
}

- (PasswordNoteCell*)noteCell {
  NSIndexPath* noteIndexPath =
      [NSIndexPath indexPathForRow:0 inSection:SectionIdentifierNote];
  PasswordNoteCell* noteCell =
      [self.tableView cellForRowAtIndexPath:noteIndexPath];

  return noteCell;
}

#pragma mark - PasswordNoteCellDelegate

- (void)textViewDidChangeInCell:(PasswordNoteCell*)cell {
  self.noteText = cell.textView.text;
  int noteLength = cell.textView.text.length;
  BOOL noteValid =
      noteLength <= password_manager::constants::kMaxPasswordNoteLength;
  [cell setValid:noteValid];
  [self updateSaveButtonState];

  // Notify that the character limit has been reached via VoiceOver.
  if (!noteValid) {
    NSString* tooLongNoteLocalizedString = NSLocalizedString(
        @"IDS_IOS_CREDENTIAL_PROVIDER_TOO_LONG_NOTE",
        @"Warning about the character limit for password notes");
    NSString* characterLimitString =
        base::SysUTF16ToNSString(base::NumberToString16(
            password_manager::constants::kMaxPasswordNoteLength));
    UIAccessibilityPostNotification(
        UIAccessibilityAnnouncementNotification,
        NSLocalizedString(
            [tooLongNoteLocalizedString
                stringByReplacingOccurrencesOfString:@"$1"
                                          withString:characterLimitString],
            @"Warning about the character limit for password notes."));
  }

  // Update note footer based on note's length.
  self.isNoteFooterShown = noteLength >= kMinNoteCharAmountForWarning;
  UITableViewHeaderFooterView* footerView =
      [self.tableView footerViewForSection:SectionIdentifierNote];
  PasswordNoteFooterView* noteFooter =
      base::apple::ObjCCastStrict<PasswordNoteFooterView>(footerView);
  noteFooter.textLabel.text = [self noteFooterText];

  // Refresh the cell's height to make the note fully visible while typing or to
  // clear unnecessary blank lines while removing characters.
  [self.tableView beginUpdates];
  [self.tableView endUpdates];
}

#pragma mark - NewPasswordTableCellDelegate

- (void)textFieldDidBeginEditingInCell:(NewPasswordTableCell*)cell {
  self.accessoryView.previousButton.enabled = (cell == self.passwordCell);
  self.accessoryView.nextButton.enabled = (cell == self.usernameCell);
}

- (void)textFieldDidChangeInCell:(NewPasswordTableCell*)cell {
  if (cell == self.passwordCell) {
    self.passwordText = cell.textField.text;
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
  } else if (cell == self.usernameCell) {
    self.usernameText = cell.textField.text;
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
      self.passwordText.length > 0 &&
      self.noteText.length <=
          password_manager::constants::kMaxPasswordNoteLength;
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

// Saves the current data as a credential. If `shouldReplace` is YES, then the
// user has already said they are aware that they are replacing a previous
// credential.
- (void)saveCredential:(BOOL)shouldReplace {
  NSString* username = self.usernameText;
  NSString* password = self.passwordText;
  NSString* note = self.noteText;

  // TODO(crbug.com/330355124): Get the gaia ID if there's only 1 account OR
  // show some UI so that the user can pick which account to create the password
  // in.
  NSString* gaia = nil;

  [self.credentialHandler saveCredentialWithUsername:username
                                            password:password
                                                note:note
                                                gaia:gaia
                                       shouldReplace:shouldReplace];
}

- (NSString*)noteFooterText {
  if (self.isNoteFooterShown) {
    NSString* tooLongNoteLocalizedString = NSLocalizedString(
        @"IDS_IOS_CREDENTIAL_PROVIDER_TOO_LONG_NOTE",
        @"Warning about the character limit for password notes");
    NSString* characterLimitString =
        base::SysUTF16ToNSString(base::NumberToString16(
            password_manager::constants::kMaxPasswordNoteLength));
    return [tooLongNoteLocalizedString
        stringByReplacingOccurrencesOfString:@"$1"
                                  withString:characterLimitString];
  }

  return @"";
}

#pragma mark - NewPasswordUIHandler

- (void)setPassword:(NSString*)password {
  NewPasswordTableCell* passwordCell = self.passwordCell;
  passwordCell.textField.text = password;
  self.passwordText = password;
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
  NSString* username = self.usernameText;
  NSString* message = [[messageBaseLocalizedString
      stringByReplacingOccurrencesOfString:@"$2"
                                withString:self.currentHost]
      stringByReplacingOccurrencesOfString:@"$1"
                                withString:username ?: @""];
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
      (credential.username.length)
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
                          initWithCloseButtonTitle:
                              NSLocalizedString(
                                  @"IDS_IOS_CREDENTIAL_PROVIDER_DONE", @"Done")
                     closeButtonAccessibilityLabel:
                         NSLocalizedString(@"IDS_IOS_CREDENTIAL_PROVIDER_NEW_"
                                           @"PASSWORD_HIDE_KEYBOARD_HINT",
                                           @"Hide Keyboard")
                      nextButtonAccessibilityLabel:
                          NSLocalizedString(@"IDS_IOS_CREDENTIAL_PROVIDER_NEW_"
                                            @"PASSWORD_NEXT_FIELD_HINT",
                                            @"Next field")
                  previousButtonAccessibilityLabel:
                      NSLocalizedString(@"IDS_IOS_CREDENTIAL_PROVIDER_NEW_"
                                        @"PASSWORD_PREVIOUS_FIELD_HINT",
                                        @"Previous field")
                manualFillButtonAccessibilityLabel:nil
        passwordManualFillButtonAccessibilityLabel:nil
      creditCardManualFillButtonAccessibilityLabel:nil
         addressManualFillButtonAccessibilityLabel:nil];
}

- (void)fromInputAccessoryViewDidTapOmniboxTypingShield:
    (FormInputAccessoryView*)sender {
  NOTREACHED_IN_MIGRATION()
      << "The typing shield should only be present on web";
}

@end
