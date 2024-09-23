// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_details/add_password_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/ios/ios_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/password_manager_metrics_util.h"
#import "components/password_manager/core/common/password_manager_constants.h"
#import "ios/chrome/browser/keyboard/ui_bundled/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_multi_line_text_edit_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_multi_line_text_edit_item_delegate.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item_delegate.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/ui/settings/password/password_details/add_password_view_controller_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_details/credential_details.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_constants.h"
#import "ios/chrome/browser/ui/settings/password/passwords_table_view_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/popover_label_view_controller.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

using password_manager::constants::kMaxPasswordNoteLength;
using password_manager::metrics_util::PasswordCheckInteraction;

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierPassword = kSectionIdentifierEnumZero,
  SectionIdentifierSite,
  SectionIdentifierDuplicate,
  SectionIdentifierFooter,
  SectionIdentifierTLDFooter,
  SectionIdentifierNoteFooter
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeWebsite = kItemTypeEnumZero,
  ItemTypeUsername,
  ItemTypePassword,
  ItemTypeFooter,
  ItemTypeNote,
  ItemTypeDuplicateCredentialButton,
  ItemTypeDuplicateCredentialMessage
};

// Size of the symbols.
const CGFloat kSymbolSize = 15;
// Minimal amount of characters in password note to display the warning.
const int kMinNoteCharAmountForWarning = 901;

}  // namespace

@interface AddPasswordViewController () <TableViewTextEditItemDelegate,
                                         TableViewMultiLineTextEditItemDelegate>

// Whether the password is shown in plain text form or in masked form.
@property(nonatomic, assign, getter=isPasswordShown) BOOL passwordShown;

// The text item related to the site value.
@property(nonatomic, strong) TableViewTextEditItem* websiteTextItem;

// The text item related to the username value.
@property(nonatomic, strong) TableViewTextEditItem* usernameTextItem;

// The text item related to the password value.
@property(nonatomic, strong) TableViewTextEditItem* passwordTextItem;

// The text item related to the password note value.
@property(nonatomic, strong) TableViewMultiLineTextEditItem* noteTextItem;

// The view used to anchor error alert which is shown for the username. This is
// image icon in the `usernameTextItem` cell.
@property(nonatomic, weak) UIView* usernameErrorAnchorView;

// If YES, denotes that the credential with the same website/username
// combination already exists. Used when creating a new credential.
@property(nonatomic, assign) BOOL isDuplicatedCredential;

// Denotes that the save button in the add credential view can be enabled after
// basic validation of data on all the fields. Does not account for whether the
// duplicate credential exists or not.
@property(nonatomic, assign) BOOL shouldEnableSave;

// Yes, when the message for top-level domain missing is shown.
@property(nonatomic, assign) BOOL isTLDMissingMessageShown;

// Yes, when the footer informing about the max note length is shown.
@property(nonatomic, assign) BOOL isNoteFooterShown;

// Yes, when the note's length is less or equal than
// `password_manager::constants::kMaxPasswordNoteLength`.
@property(nonatomic, assign) BOOL isNoteValid;

// If YES, the password details are shown without requiring any authentication.
@property(nonatomic, assign) BOOL showPasswordWithoutAuth;

// The account where passwords are being saved to, or nil if passwords are only
// being saved locally.
@property(nonatomic, strong) NSString* accountSavingPasswords;

// Stores the user current typed password. (Used for testing).
@property(nonatomic, strong) NSString* passwordForTesting;

@end

@implementation AddPasswordViewController

#pragma mark - ViewController Life Cycle.

- (instancetype)init {
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _isDuplicatedCredential = NO;
    _shouldEnableSave = NO;
    _showPasswordWithoutAuth = NO;
    _isTLDMissingMessageShown = NO;
    _isNoteFooterShown = NO;
    _isNoteValid = YES;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.accessibilityIdentifier = kPasswordDetailsViewControllerID;
  self.tableView.allowsSelectionDuringEditing = YES;

  self.navigationItem.title = l10n_util::GetNSString(
      IDS_IOS_PASSWORD_SETTINGS_ADD_PASSWORD_MANUALLY_TITLE);

  // Adds 'Cancel' and 'Save' buttons to Navigation bar.
  self.navigationItem.leftBarButtonItem = [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_NAVIGATION_BAR_CANCEL_BUTTON)
              style:UIBarButtonItemStylePlain
             target:self
             action:@selector(didTapCancelButton:)];
  self.navigationItem.leftBarButtonItem.accessibilityIdentifier =
      kPasswordsAddPasswordCancelButtonID;

  self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(
                        IDS_IOS_PASSWORD_SETTINGS_SAVE_BUTTON)
              style:UIBarButtonItemStyleDone
             target:self
             action:@selector(didTapSaveButton:)];
  self.navigationItem.rightBarButtonItem.enabled = NO;
  self.navigationItem.rightBarButtonItem.accessibilityIdentifier =
      kPasswordsAddPasswordSaveButtonID;

  password_manager::metrics_util::
      LogUserInteractionsWhenAddingCredentialFromSettings(
          password_manager::metrics_util::
              AddCredentialFromSettingsUserInteractions::kAddDialogOpened);

  [self loadModel];
}

- (void)viewDidDisappear:(BOOL)animated {
  password_manager::metrics_util::
      LogUserInteractionsWhenAddingCredentialFromSettings(
          password_manager::metrics_util::
              AddCredentialFromSettingsUserInteractions::kAddDialogClosed);
  [super viewDidDisappear:animated];
}

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;

  self.websiteTextItem = [self websiteItem];

  [model addSectionWithIdentifier:SectionIdentifierSite];

  [model addItem:self.websiteTextItem
      toSectionWithIdentifier:SectionIdentifierSite];

  [model addSectionWithIdentifier:SectionIdentifierTLDFooter];

  [model addSectionWithIdentifier:SectionIdentifierPassword];

  self.usernameTextItem = [self usernameItem];
  [model addItem:self.usernameTextItem
      toSectionWithIdentifier:SectionIdentifierPassword];

  self.passwordTextItem = [self passwordItem];
  [model addItem:self.passwordTextItem
      toSectionWithIdentifier:SectionIdentifierPassword];

  self.noteTextItem = [self noteItem];
  [model addItem:self.noteTextItem
      toSectionWithIdentifier:SectionIdentifierPassword];
  [model addSectionWithIdentifier:SectionIdentifierNoteFooter];

  [model addSectionWithIdentifier:SectionIdentifierFooter];
  [model setFooter:[self footerItem]
      forSectionWithIdentifier:SectionIdentifierFooter];
}

- (BOOL)showCancelDuringEditing {
  return YES;
}

#pragma mark - Items

- (TableViewTextEditItem*)websiteItem {
  TableViewTextEditItem* item =
      [[TableViewTextEditItem alloc] initWithType:ItemTypeWebsite];
  item.textFieldBackgroundColor = [UIColor clearColor];
  item.fieldNameLabelText =
      l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_SITE);
  item.textFieldEnabled = YES;
  item.autoCapitalizationType = UITextAutocapitalizationTypeNone;
  item.hideIcon = NO;
  item.keyboardType = UIKeyboardTypeURL;
  item.textFieldPlaceholder = l10n_util::GetNSString(
      IDS_IOS_PASSWORD_SETTINGS_WEBSITE_PLACEHOLDER_TEXT);
  item.delegate = self;
  return item;
}

- (TableViewTextEditItem*)usernameItem {
  TableViewTextEditItem* item =
      [[TableViewTextEditItem alloc] initWithType:ItemTypeUsername];
  item.textFieldBackgroundColor = [UIColor clearColor];
  item.fieldNameLabelText =
      l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_USERNAME);
  item.textFieldEnabled = YES;
  item.hideIcon = NO;
  item.autoCapitalizationType = UITextAutocapitalizationTypeNone;
  item.delegate = self;
  item.textFieldPlaceholder = l10n_util::GetNSString(
      IDS_IOS_PASSWORD_SETTINGS_USERNAME_PLACEHOLDER_TEXT);
  return item;
}

- (TableViewTextEditItem*)passwordItem {
  TableViewTextEditItem* item =
      [[TableViewTextEditItem alloc] initWithType:ItemTypePassword];
  item.textFieldBackgroundColor = [UIColor clearColor];
  item.fieldNameLabelText =
      l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_PASSWORD);
  item.textFieldSecureTextEntry = ![self isPasswordShown];
  item.textFieldEnabled = YES;
  item.hideIcon = NO;
  item.autoCapitalizationType = UITextAutocapitalizationTypeNone;
  item.keyboardType = UIKeyboardTypeURL;
  item.returnKeyType = UIReturnKeyDone;
  item.delegate = self;
  item.textFieldPlaceholder = l10n_util::GetNSString(
      IDS_IOS_PASSWORD_SETTINGS_PASSWORD_PLACEHOLDER_TEXT);

  // During editing password is exposed so eye icon shouldn't be shown.
  if (!self.tableView.editing) {
    UIImage* image =
        [self isPasswordShown]
            ? DefaultSymbolWithPointSize(kHideActionSymbol, kSymbolSize)
            : DefaultSymbolWithPointSize(kShowActionSymbol, kSymbolSize);
    item.identifyingIcon = image;
    item.identifyingIconEnabled = YES;
    item.identifyingIconAccessibilityLabel = l10n_util::GetNSString(
        [self isPasswordShown] ? IDS_IOS_SETTINGS_PASSWORD_HIDE_BUTTON
                               : IDS_IOS_SETTINGS_PASSWORD_SHOW_BUTTON);
  }
  return item;
}

- (TableViewMultiLineTextEditItem*)noteItem {
  TableViewMultiLineTextEditItem* item =
      [[TableViewMultiLineTextEditItem alloc] initWithType:ItemTypeNote];
  item.label = l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_NOTE);
  item.editingEnabled = YES;
  item.delegate = self;
  return item;
}

- (TableViewTextItem*)duplicatePasswordViewButtonItem {
  TableViewTextItem* item = [[TableViewTextItem alloc]
      initWithType:ItemTypeDuplicateCredentialButton];
  item.text =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_SETTINGS_VIEW_PASSWORD_BUTTON);
  item.textColor = [UIColor colorNamed:kBlueColor];
  item.accessibilityTraits = UIAccessibilityTraitButton;
  return item;
}

- (SettingsImageDetailTextItem*)duplicatePasswordMessageItem {
  SettingsImageDetailTextItem* item = [[SettingsImageDetailTextItem alloc]
      initWithType:ItemTypeDuplicateCredentialMessage];
  if (self.usernameTextItem &&
      [self.usernameTextItem.textFieldValue length] > 0) {
    item.detailText = l10n_util::GetNSStringF(
        IDS_IOS_SETTINGS_PASSWORDS_DUPLICATE_SECTION_ALERT_DESCRIPTION,
        base::SysNSStringToUTF16(self.usernameTextItem.textFieldValue),
        base::SysNSStringToUTF16(self.websiteTextItem.textFieldValue));
  } else {
    item.detailText = l10n_util::GetNSStringF(
        IDS_IOS_SETTINGS_PASSWORDS_DUPLICATE_SECTION_ALERT_DESCRIPTION_WITHOUT_USERNAME,
        base::SysNSStringToUTF16(self.websiteTextItem.textFieldValue));
  }
  item.image = DefaultSymbolWithPointSize(kErrorCircleFillSymbol, kSymbolSize);
  item.imageViewTintColor = [UIColor colorNamed:kRedColor];
  return item;
}

- (TableViewLinkHeaderFooterItem*)footerItem {
  TableViewLinkHeaderFooterItem* item =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeFooter];
  item.text =
      [NSString stringWithFormat:@"%@\n\n%@",
                                 l10n_util::GetNSString(
                                     IDS_IOS_SETTINGS_ADD_PASSWORD_DESCRIPTION),
                                 [self footerText]];
  return item;
}

- (TableViewLinkHeaderFooterItem*)TLDMessageFooterItem {
  TableViewLinkHeaderFooterItem* item =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeFooter];
  item.text = l10n_util::GetNSStringF(
      IDS_IOS_SETTINGS_PASSWORDS_MISSING_TLD_DESCRIPTION,
      base::SysNSStringToUTF16([self.websiteTextItem.textFieldValue
          stringByAppendingString:@".com"]));
  return item;
}

- (TableViewLinkHeaderFooterItem*)tooLongNoteMessageFooterItem {
  TableViewLinkHeaderFooterItem* item =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeFooter];
  item.text = l10n_util::GetNSStringF(
      IDS_IOS_SETTINGS_PASSWORDS_TOO_LONG_NOTE_DESCRIPTION,
      base::NumberToString16(
          password_manager::constants::kMaxPasswordNoteLength));
  return item;
}

- (NSString*)footerText {
  if (self.accountSavingPasswords) {
    return l10n_util::GetNSStringF(
        IDS_IOS_SETTINGS_ADD_PASSWORD_FOOTER_BRANDED,
        base::SysNSStringToUTF16(self.accountSavingPasswords));
  }

  return l10n_util::GetNSString(IDS_IOS_SAVE_PASSWORD_FOOTER_NOT_SYNCING);
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  TableViewModel* model = self.tableViewModel;
  NSInteger itemType = [model itemTypeForIndexPath:indexPath];
  if (itemType == ItemTypeNote) {
    UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:indexPath];
    TableViewMultiLineTextEditCell* textFieldCell =
        base::apple::ObjCCastStrict<TableViewMultiLineTextEditCell>(cell);
    [textFieldCell.textView becomeFirstResponder];
    return;
  }

  if (itemType != ItemTypeDuplicateCredentialButton) {
    return;
  }

  password_manager::metrics_util::
      LogUserInteractionsWhenAddingCredentialFromSettings(
          password_manager::metrics_util::
              AddCredentialFromSettingsUserInteractions::
                  kDuplicateCredentialViewed);

    NSString* usernameTextValue = _usernameTextItem.textFieldValue;
    [_delegate showExistingCredential:usernameTextValue];
}

- (UITableViewCellEditingStyle)tableView:(UITableView*)tableView
           editingStyleForRowAtIndexPath:(NSIndexPath*)indexPath {
  return UITableViewCellEditingStyleNone;
}

- (BOOL)tableView:(UITableView*)tableview
    shouldIndentWhileEditingRowAtIndexPath:(NSIndexPath*)indexPath {
  return NO;
}

- (BOOL)tableView:(UITableView*)tableView
    shouldHighlightRowAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  return itemType == ItemTypeDuplicateCredentialButton ||
         itemType == ItemTypeNote;
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  NSInteger sectionIdentifier =
      [self.tableViewModel sectionIdentifierForSectionIndex:section];

  if (sectionIdentifier == SectionIdentifierFooter ||
      sectionIdentifier == SectionIdentifierTLDFooter) {
    return 0;
  }
  return [super tableView:tableView heightForHeaderInSection:section];
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  NSInteger sectionIdentifier =
      [self.tableViewModel sectionIdentifierForSectionIndex:section];
  if (sectionIdentifier == SectionIdentifierSite) {
    return 0;
  }
  if ((sectionIdentifier == SectionIdentifierPassword &&
       !self.isDuplicatedCredential) ||
      sectionIdentifier == SectionIdentifierDuplicate) {
    return 0;
  }
  return [super tableView:tableView heightForFooterInSection:section];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];

  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  cell.tag = itemType;
  cell.selectionStyle = UITableViewCellSelectionStyleDefault;

  switch (itemType) {
    case ItemTypeUsername: {
      TableViewTextEditCell* textFieldCell =
          base::apple::ObjCCastStrict<TableViewTextEditCell>(cell);
      textFieldCell.textField.delegate = self;
      break;
    }
    case ItemTypePassword: {
      TableViewTextEditCell* textFieldCell =
          base::apple::ObjCCastStrict<TableViewTextEditCell>(cell);
      textFieldCell.textField.delegate = self;
      [textFieldCell.identifyingIconButton
                 addTarget:self
                    action:@selector(didTapShowHideButton:)
          forControlEvents:UIControlEventTouchUpInside];
      break;
    }
    case ItemTypeWebsite: {
      TableViewTextEditCell* textFieldCell =
          base::apple::ObjCCastStrict<TableViewTextEditCell>(cell);
      textFieldCell.textField.delegate = self;
      break;
    }
    case ItemTypeDuplicateCredentialButton: {
      cell.selectionStyle = UITableViewCellSelectionStyleNone;
      break;
    }
    case ItemTypeNote: {
      cell.selectionStyle = UITableViewCellSelectionStyleNone;
      break;
    }
    case ItemTypeDuplicateCredentialMessage:
    case ItemTypeFooter:
      break;
  }
  return cell;
}

- (BOOL)tableView:(UITableView*)tableView
    canEditRowAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case ItemTypeWebsite:
    case ItemTypeFooter:
    case ItemTypeDuplicateCredentialMessage:
    case ItemTypeDuplicateCredentialButton:
      return NO;
    case ItemTypeUsername:
    case ItemTypePassword:
    case ItemTypeNote:
      return YES;
  }
  return NO;
}

#pragma mark - AddPasswordDetailsConsumer

- (void)setAccountSavingPasswords:(NSString*)accountSavingPasswords {
  _accountSavingPasswords = accountSavingPasswords;
}

- (void)onDuplicateCheckCompletion:(BOOL)duplicateFound {
  if (duplicateFound == self.isDuplicatedCredential) {
    return;
  }

  self.isDuplicatedCredential = duplicateFound;
  [self toggleNavigationBarRightButtonItem];
  TableViewModel* model = self.tableViewModel;
  if (duplicateFound) {
    password_manager::metrics_util::
        LogUserInteractionsWhenAddingCredentialFromSettings(
            password_manager::metrics_util::
                AddCredentialFromSettingsUserInteractions::
                    kDuplicatedCredentialEntered);
    [self
        performBatchTableViewUpdates:^{
          NSUInteger passwordSectionIndex = [self.tableViewModel
              sectionForSectionIdentifier:SectionIdentifierPassword];
          [model insertSectionWithIdentifier:SectionIdentifierDuplicate
                                     atIndex:passwordSectionIndex + 1];
          [self.tableView
                insertSections:[NSIndexSet
                                   indexSetWithIndex:passwordSectionIndex + 1]
              withRowAnimation:UITableViewRowAnimationTop];
          [model addItem:[self duplicatePasswordMessageItem]
              toSectionWithIdentifier:SectionIdentifierDuplicate];
          [model addItem:[self duplicatePasswordViewButtonItem]
              toSectionWithIdentifier:SectionIdentifierDuplicate];
          if (self.usernameTextItem &&
              [self.usernameTextItem.textFieldValue length] > 0) {
            self.usernameTextItem.hasValidText = NO;
            [self reconfigureCellsForItems:@[ self.usernameTextItem ]];
          } else {
            self.websiteTextItem.hasValidText = NO;
            [self reconfigureCellsForItems:@[ self.websiteTextItem ]];
          }
        }
                          completion:nil];
  } else {
    [self
        performBatchTableViewUpdates:^{
          [self removeSectionWithIdentifier:SectionIdentifierDuplicate
                           withRowAnimation:UITableViewRowAnimationTop];
          self.usernameTextItem.hasValidText = YES;
          self.websiteTextItem.hasValidText = YES;
          [self reconfigureCellsForItems:@[
            self.websiteTextItem, self.usernameTextItem
          ]];
        }
                          completion:nil];
  }
}

#pragma mark - TableViewTextEditItemDelegate

- (void)tableViewItemDidBeginEditing:(TableViewTextEditItem*)tableViewItem {
  [self reconfigureCellsForItems:@[
    self.websiteTextItem, self.usernameTextItem, self.passwordTextItem
  ]];
}

- (void)tableViewItemDidChange:(TableViewTextEditItem*)tableViewItem {
  if (tableViewItem == self.websiteTextItem) {
    [self.delegate setWebsiteURL:self.websiteTextItem.textFieldValue];
    if (self.isTLDMissingMessageShown) {
      self.isTLDMissingMessageShown = NO;
      [self
          performBatchTableViewUpdates:^{
            [self.tableViewModel setFooter:nil
                  forSectionWithIdentifier:SectionIdentifierTLDFooter];
            NSUInteger index = [self.tableViewModel
                sectionForSectionIdentifier:SectionIdentifierTLDFooter];
            [self.tableView reloadSections:[NSIndexSet indexSetWithIndex:index]
                          withRowAnimation:UITableViewRowAnimationNone];
          }
                            completion:nil];
    }
  }

  BOOL siteValid = [self checkIfValidSite];
  BOOL passwordValid = [self checkIfValidPassword];

  self.shouldEnableSave = (siteValid && passwordValid && self.isNoteValid);
  [self toggleNavigationBarRightButtonItem];

  [self.delegate checkForDuplicates:self.usernameTextItem.textFieldValue];
}

- (void)tableViewItemDidEndEditing:(TableViewTextEditItem*)tableViewItem {
  if (tableViewItem == self.websiteTextItem) {
    if (!self.isDuplicatedCredential) {
      self.websiteTextItem.hasValidText = [self checkIfValidSite];
    }
    if ([self.websiteTextItem.textFieldValue length] > 0 &&
        [self.delegate isTLDMissing]) {
      [self showTLDMissingSection];
      self.websiteTextItem.hasValidText = NO;
    }
    [self reconfigureCellsForItems:@[ self.websiteTextItem ]];
  } else if (tableViewItem == self.usernameTextItem) {
    [self reconfigureCellsForItems:@[ self.usernameTextItem ]];
  } else if (tableViewItem == self.passwordTextItem) {
    self.passwordTextItem.hasValidText = [self checkIfValidPassword];
    [self reconfigureCellsForItems:@[ self.passwordTextItem ]];
  }
}

#pragma mark - TableViewMultiLineTextEditItemDelegate

- (void)textViewItemDidChange:(TableViewMultiLineTextEditItem*)tableViewItem {
  DCHECK(tableViewItem == self.noteTextItem);

  // Update save button state based on the note's length and validity of other
  // input fields.
  BOOL noteValid = tableViewItem.text.length <= kMaxPasswordNoteLength;
  if (self.isNoteValid != noteValid) {
    self.isNoteValid = noteValid;
    tableViewItem.validText = noteValid;

    self.shouldEnableSave =
        noteValid && [self checkIfValidSite] && [self checkIfValidPassword];
    [self toggleNavigationBarRightButtonItem];
  }

  // Notify that the note character limit has been reached via VoiceOver.
  if (!noteValid) {
    NSString* tooLongNoteMessage = l10n_util::GetNSStringF(
        IDS_IOS_SETTINGS_PASSWORDS_TOO_LONG_NOTE_DESCRIPTION,
        base::NumberToString16(kMaxPasswordNoteLength));
    UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification,
                                    tooLongNoteMessage);
  }

  // Update note footer based on the note's length.
  BOOL shouldDisplayNoteFooter =
      tableViewItem.text.length >= kMinNoteCharAmountForWarning;
  if (self.isNoteFooterShown != shouldDisplayNoteFooter) {
    self.isNoteFooterShown = shouldDisplayNoteFooter;
    [self
        performBatchTableViewUpdates:^{
          [self.tableViewModel
                             setFooter:shouldDisplayNoteFooter
                                           ? [self tooLongNoteMessageFooterItem]
                                           : nil
              forSectionWithIdentifier:SectionIdentifierNoteFooter];
          NSUInteger index = [self.tableViewModel
              sectionForSectionIdentifier:SectionIdentifierNoteFooter];
          [self.tableView reloadSections:[NSIndexSet indexSetWithIndex:index]
                        withRowAnimation:UITableViewRowAnimationNone];
        }
                          completion:nil];
  }

  [self reconfigureCellsForItems:@[ tableViewItem ]];

  // Refresh the cells' height.
  [self.tableView beginUpdates];
  [self.tableView endUpdates];
}

#pragma mark - Actions

// Dimisses this view controller when Cancel button is tapped.
- (void)didTapCancelButton:(id)sender {
  [self.delegate didCancelAddPasswordDetails];
}

// Handles Save button tap on adding new credentials.
- (void)didTapSaveButton:(id)sender {
  if ([self.websiteTextItem.textFieldValue length] > 0 &&
      [self.delegate isTLDMissing]) {
    [self showTLDMissingSection];
    return;
  }
  password_manager::metrics_util::
      LogUserInteractionsWhenAddingCredentialFromSettings(
          password_manager::metrics_util::
              AddCredentialFromSettingsUserInteractions::kCredentialAdded);
  base::RecordAction(
      base::UserMetricsAction("MobilePasswordManagerAddPassword"));
  if (self.noteTextItem.text.length != 0) {
    password_manager::metrics_util::LogPasswordNoteActionInSettings(
        password_manager::metrics_util::PasswordNoteAction::
            kNoteAddedInAddDialog);
  }
  [self.delegate addPasswordViewController:self
                     didAddPasswordDetails:self.usernameTextItem.textFieldValue
                                  password:self.passwordTextItem.textFieldValue
                                      note:self.noteTextItem.text];
}

#pragma mark - SettingsRootTableViewController

- (BOOL)shouldHideToolbar {
  return YES;
}

#pragma mark - AutofillEditTableViewController

- (BOOL)isItemAtIndexPathTextEditCell:(NSIndexPath*)cellPath {
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:cellPath];
  switch (static_cast<ItemType>(itemType)) {
    case ItemTypeUsername:
    case ItemTypePassword:
    case ItemTypeWebsite:
      return YES;
    case ItemTypeDuplicateCredentialMessage:
    case ItemTypeDuplicateCredentialButton:
    case ItemTypeFooter:
    case ItemTypeNote:
      return NO;
  };
}

#pragma mark - Private

- (BOOL)checkIfValidSite {
  BOOL siteEmpty = [self.websiteTextItem.textFieldValue length] == 0;
  if (!siteEmpty && !self.isTLDMissingMessageShown &&
      !self.isDuplicatedCredential) {
    self.websiteTextItem.hasValidText = YES;
    [self reconfigureCellsForItems:@[ self.websiteTextItem ]];
  }
  return !siteEmpty;
}

// Checks if the password is valid and updates item accordingly.
- (BOOL)checkIfValidPassword {
  BOOL passwordEmpty = [self.passwordTextItem.textFieldValue length] == 0;
  if (!passwordEmpty) {
    self.passwordTextItem.hasValidText = YES;
    [self reconfigureCellsForItems:@[ self.passwordTextItem ]];
  }

  return !passwordEmpty;
}

// Removes the given section if it exists.
- (void)removeSectionWithIdentifier:(NSInteger)sectionIdentifier
                   withRowAnimation:(UITableViewRowAnimation)animation {
  TableViewModel* model = self.tableViewModel;
  if ([model hasSectionForSectionIdentifier:sectionIdentifier]) {
    NSInteger section = [model sectionForSectionIdentifier:sectionIdentifier];
    [model removeSectionWithIdentifier:sectionIdentifier];
    [[self tableView] deleteSections:[NSIndexSet indexSetWithIndex:section]
                    withRowAnimation:animation];
  }
}

// Enables/Disables the right bar button item in the navigation bar.
- (void)toggleNavigationBarRightButtonItem {
  self.navigationItem.rightBarButtonItem.enabled =
      !self.isDuplicatedCredential && self.shouldEnableSave &&
      [self.delegate isURLValid] && !self.isTLDMissingMessageShown;
}

// Shows the section with the error message for top-level domain missing.
- (void)showTLDMissingSection {
  if (self.isTLDMissingMessageShown) {
    return;
  }

  self.navigationItem.rightBarButtonItem.enabled = NO;
  self.isTLDMissingMessageShown = YES;
  [self
      performBatchTableViewUpdates:^{
        [self.tableViewModel setFooter:[self TLDMessageFooterItem]
              forSectionWithIdentifier:SectionIdentifierTLDFooter];
        NSUInteger index = [self.tableViewModel
            sectionForSectionIdentifier:SectionIdentifierTLDFooter];
        [self.tableView reloadSections:[NSIndexSet indexSetWithIndex:index]
                      withRowAnimation:UITableViewRowAnimationNone];
      }
                        completion:nil];
}

#pragma mark - Actions

// Called when the user tapped on the show/hide button near password.
- (void)didTapShowHideButton:(UIButton*)buttonView {
  [self.tableView deselectRowAtIndexPath:self.tableView.indexPathForSelectedRow
                                animated:NO];
  if (self.isPasswordShown) {
    self.passwordShown = NO;
    self.passwordTextItem.textFieldSecureTextEntry = YES;
    // Only change the textFieldValue for tests.
    if (self.passwordForTesting) {
      self.passwordTextItem.textFieldValue = kMaskedPassword;
    }
    self.passwordTextItem.identifyingIcon =
        DefaultSymbolWithPointSize(kShowActionSymbol, kSymbolSize);
    self.passwordTextItem.identifyingIconAccessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_SHOW_BUTTON);
    [self reconfigureCellsForItems:@[ self.passwordTextItem ]];
  } else {
    self.passwordTextItem.textFieldSecureTextEntry = NO;
    self.passwordShown = YES;
    // Only change the textFieldValue for tests.
    if (self.passwordForTesting) {
      self.passwordTextItem.textFieldValue = self.passwordForTesting;
    }
    self.passwordTextItem.identifyingIcon =
        DefaultSymbolWithPointSize(kHideActionSymbol, kSymbolSize);
    self.passwordTextItem.identifyingIconAccessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_HIDE_BUTTON);
    [self reconfigureCellsForItems:@[ self.passwordTextItem ]];
  }
}

// Called when the user tap error info icon in the username input.
- (void)didTapUsernameErrorInfo:(UIButton*)buttonView {
  NSString* text = l10n_util::GetNSString(IDS_IOS_USERNAME_ALREADY_USED);

  NSAttributedString* attributedText = [[NSAttributedString alloc]
      initWithString:text
          attributes:@{
            NSForegroundColorAttributeName :
                [UIColor colorNamed:kTextSecondaryColor],
            NSFontAttributeName :
                [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline]
          }];

  PopoverLabelViewController* errorInfoPopover =
      [[PopoverLabelViewController alloc]
          initWithPrimaryAttributedString:attributedText
                secondaryAttributedString:nil];

  errorInfoPopover.popoverPresentationController.sourceView =
      self.usernameErrorAnchorView;
  errorInfoPopover.popoverPresentationController.sourceRect =
      self.usernameErrorAnchorView.bounds;
  errorInfoPopover.popoverPresentationController.permittedArrowDirections =
      UIPopoverArrowDirectionAny;
  [self presentViewController:errorInfoPopover animated:YES completion:nil];
}

#pragma mark - ForTesting

- (void)setPassword:(NSString*)password {
  NSIndexPath* indexPath =
      [self.tableViewModel indexPathForItem:self.passwordTextItem];
  TableViewTextEditItem* item = static_cast<TableViewTextEditItem*>(
      [self.tableViewModel itemAtIndexPath:indexPath]);
  self.passwordForTesting = password;
  item.textFieldValue = [self isPasswordShown] || self.tableView.editing
                            ? self.passwordForTesting
                            : kMaskedPassword;
}

#pragma mark - UIResponder

// To always be able to register key commands via -keyCommands, the VC must be
// able to become first responder.
- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (NSArray*)keyCommands {
  return @[ UIKeyCommand.cr_close ];
}

- (void)keyCommand_close {
  base::RecordAction(base::UserMetricsAction("MobileKeyCommandClose"));
  [self didTapCancelButton:nil];
}

@end
