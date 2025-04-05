// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/password/password_details/add_password_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/ios/ios_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/generation/password_generator.h"
#import "components/password_manager/core/browser/password_manager_metrics_util.h"
#import "components/password_manager/core/browser/password_requirements_service.h"
#import "components/password_manager/core/common/password_manager_constants.h"
#import "ios/chrome/browser/keyboard/ui_bundled/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/settings/ui_bundled/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_details/add_password_view_controller_delegate.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_details/credential_details.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_details/password_details_table_view_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_manager_ui_features.h"
#import "ios/chrome/browser/settings/ui_bundled/password/passwords_table_view_constants.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_multi_line_text_edit_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_multi_line_text_edit_item_delegate.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item_delegate.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/popover_label_view_controller.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

using password_manager::constants::kMaxPasswordNoteLength;

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
  ItemTypeSuggestPassword,
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

// The text item related to the username value.
@property(nonatomic, strong) TableViewTextEditItem* usernameTextItem;
// The text item related to the site value.
@property(nonatomic, strong) TableViewTextEditItem* websiteTextItem;

@end

@implementation AddPasswordViewController {
  // Whether the password is shown in plain text form or in masked form.
  BOOL _passwordShown;

  // The text item related to the password value.
  TableViewTextEditItem* _passwordTextItem;

  // The text item related to the suggest strong password value.
  TableViewTextItem* _suggestPasswordTextItem;

  // The text item related to the password note value.
  TableViewMultiLineTextEditItem* _noteTextItem;

  // The view used to anchor error alert which is shown for the username. This
  // is image icon in the `usernameTextItem` cell.
  UIView* _usernameErrorAnchorView;

  // If YES, denotes that the credential with the same website/username
  // combination already exists. Used when creating a new credential.
  BOOL _isDuplicatedCredential;

  // Denotes that the save button in the add credential view can be enabled
  // after
  // basic validation of data on all the fields. Does not account for whether
  // the duplicate credential exists or not.
  BOOL _shouldEnableSave;

  // Yes, when the message for top-level domain missing is shown.
  BOOL _isTLDMissingMessageShown;

  // Yes, when the footer informing about the max note length is shown.
  BOOL _isNoteFooterShown;

  // Yes, when the note's length is less or equal than
  // `password_manager::constants::kMaxPasswordNoteLength`.
  BOOL _isNoteValid;

  // The account where passwords are being saved to, or nil if passwords are
  // only being saved locally.
  NSString* _accountSavingPasswords;

  // Stores the user current typed password. (Used for testing).
  NSString* _passwordForTesting;
}

#pragma mark - ViewController Life Cycle.

- (instancetype)init {
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _isDuplicatedCredential = NO;
    _shouldEnableSave = NO;
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

  _websiteTextItem = [self websiteItem];

  [model addSectionWithIdentifier:SectionIdentifierSite];

  [model addItem:_websiteTextItem
      toSectionWithIdentifier:SectionIdentifierSite];

  [model addSectionWithIdentifier:SectionIdentifierTLDFooter];

  [model addSectionWithIdentifier:SectionIdentifierPassword];

  _usernameTextItem = [self usernameItem];
  [model addItem:_usernameTextItem
      toSectionWithIdentifier:SectionIdentifierPassword];

  _passwordTextItem = [self passwordItem];
  [model addItem:_passwordTextItem
      toSectionWithIdentifier:SectionIdentifierPassword];

  if (password_manager::features::
          IsSuggestStrongPasswordInAddPasswordEnabled()) {
    if ([self.delegate shouldShowSuggestPasswordItem]) {
      _suggestPasswordTextItem = [self suggestPasswordItem];
      [model addItem:_suggestPasswordTextItem
          toSectionWithIdentifier:SectionIdentifierPassword];
    }
  }

  _noteTextItem = [self noteItem];
  [model addItem:_noteTextItem
      toSectionWithIdentifier:SectionIdentifierPassword];
  [model addSectionWithIdentifier:SectionIdentifierNoteFooter];

  [model addSectionWithIdentifier:SectionIdentifierFooter];
  [model setFooter:[self footerItem]
      forSectionWithIdentifier:SectionIdentifierFooter];
}

- (BOOL)showCancelDuringEditing {
  return YES;
}

- (void)updatePasswordTextFieldValue:(NSString*)textFieldValue {
  [_passwordTextItem updateTextFieldValue:textFieldValue];
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

- (TableViewTextItem*)suggestPasswordItem {
  // `TableViewTextItem` was chosen instead of `TableViewTextButtonItem` to
  // avoid unwanted padding introduced by UIStackView.
  TableViewTextItem* item =
      [[TableViewTextItem alloc] initWithType:ItemTypeSuggestPassword];
  item.textColor = [UIColor colorNamed:kBlueColor];
  item.text = l10n_util::GetNSString(
      IDS_IOS_CREDENTIAL_PROVIDER_NEW_PASSWORD_SUGGEST_STRONG_PASSWORD);
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
  if (_usernameTextItem && [_usernameTextItem.textFieldValue length] > 0) {
    item.detailText = l10n_util::GetNSStringF(
        IDS_IOS_SETTINGS_PASSWORDS_DUPLICATE_SECTION_ALERT_DESCRIPTION,
        base::SysNSStringToUTF16(_usernameTextItem.textFieldValue),
        base::SysNSStringToUTF16(_websiteTextItem.textFieldValue));
  } else {
    item.detailText = l10n_util::GetNSStringF(
        IDS_IOS_SETTINGS_PASSWORDS_DUPLICATE_SECTION_ALERT_DESCRIPTION_WITHOUT_USERNAME,
        base::SysNSStringToUTF16(_websiteTextItem.textFieldValue));
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
      base::SysNSStringToUTF16(
          [_websiteTextItem.textFieldValue stringByAppendingString:@".com"]));
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
  if (_accountSavingPasswords) {
    return l10n_util::GetNSStringF(
        IDS_IOS_SETTINGS_ADD_PASSWORD_FOOTER_BRANDED,
        base::SysNSStringToUTF16(_accountSavingPasswords));
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
       !_isDuplicatedCredential) ||
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
    case ItemTypeSuggestPassword: {
      UITapGestureRecognizer* tapRecognizer = [[UITapGestureRecognizer alloc]
          initWithTarget:self
                  action:@selector(didTapSuggestStrongPassword:)];
      [cell addGestureRecognizer:tapRecognizer];
      cell.accessibilityTraits |= UIAccessibilityTraitButton;
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
  if (duplicateFound == _isDuplicatedCredential) {
    return;
  }

  _isDuplicatedCredential = duplicateFound;
  [self toggleNavigationBarRightButtonItem];

  __weak __typeof(self) weakSelf = self;
  if (duplicateFound) {
    password_manager::metrics_util::
        LogUserInteractionsWhenAddingCredentialFromSettings(
            password_manager::metrics_util::
                AddCredentialFromSettingsUserInteractions::
                    kDuplicatedCredentialEntered);
    [self
        performBatchTableViewUpdates:^{
          [weakSelf updateTableViewWithDuplicatesFound:YES];
        }
                          completion:nil];
  } else {
    [self
        performBatchTableViewUpdates:^{
          [weakSelf updateTableViewWithDuplicatesFound:NO];
        }
                          completion:nil];
  }
}

#pragma mark - TableViewTextEditItemDelegate

- (void)tableViewItemDidBeginEditing:(TableViewTextEditItem*)tableViewItem {
  [self reconfigureCellsForItems:@[
    _websiteTextItem, _usernameTextItem, _passwordTextItem
  ]];
}

- (void)tableViewItemDidChange:(TableViewTextEditItem*)tableViewItem {
  if (tableViewItem == _websiteTextItem) {
    [self.delegate setWebsiteURL:_websiteTextItem.textFieldValue];
    if (_isTLDMissingMessageShown) {
      _isTLDMissingMessageShown = NO;
      __weak __typeof(self) weakSelf = self;
      [self
          performBatchTableViewUpdates:^{
            [weakSelf removeTLDFooter];
          }
                            completion:nil];
    }
  }

  BOOL siteValid = [self checkIfValidSite];
  BOOL passwordValid = [self checkIfValidPassword];

  _shouldEnableSave = (siteValid && passwordValid && _isNoteValid);
  [self toggleNavigationBarRightButtonItem];

  [self.delegate checkForDuplicates:_usernameTextItem.textFieldValue];
}

- (void)tableViewItemDidEndEditing:(TableViewTextEditItem*)tableViewItem {
  if (tableViewItem == _websiteTextItem) {
    if (!_isDuplicatedCredential) {
      _websiteTextItem.hasValidText = [self checkIfValidSite];
    }
    if ([_websiteTextItem.textFieldValue length] > 0 &&
        [self.delegate isTLDMissing]) {
      [self showTLDMissingSection];
      _websiteTextItem.hasValidText = NO;
    }
    [self reconfigureCellsForItems:@[ _websiteTextItem ]];
  } else if (tableViewItem == _usernameTextItem) {
    [self reconfigureCellsForItems:@[ _usernameTextItem ]];
  } else if (tableViewItem == _passwordTextItem) {
    _passwordTextItem.hasValidText = [self checkIfValidPassword];
    [self reconfigureCellsForItems:@[ _passwordTextItem ]];
  }
}

#pragma mark - TableViewMultiLineTextEditItemDelegate

- (void)textViewItemDidChange:(TableViewMultiLineTextEditItem*)tableViewItem {
  DCHECK(tableViewItem == _noteTextItem);

  // Update save button state based on the note's length and validity of other
  // input fields.
  BOOL noteValid = tableViewItem.text.length <= kMaxPasswordNoteLength;
  if (_isNoteValid != noteValid) {
    _isNoteValid = noteValid;
    tableViewItem.validText = noteValid;

    _shouldEnableSave =
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
  if (_isNoteFooterShown != shouldDisplayNoteFooter) {
    _isNoteFooterShown = shouldDisplayNoteFooter;
    __weak __typeof(self) weakSelf = self;
    [self
        performBatchTableViewUpdates:^{
          [weakSelf updateNoteFooterVisibility:shouldDisplayNoteFooter];
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
  if ([_websiteTextItem.textFieldValue length] > 0 &&
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
  if (_noteTextItem.text.length != 0) {
    password_manager::metrics_util::LogPasswordNoteActionInSettings(
        password_manager::metrics_util::PasswordNoteAction::
            kNoteAddedInAddDialog);
  }

  [self.delegate addPasswordViewController:self
                     didAddPasswordDetails:_usernameTextItem.textFieldValue
                                  password:_passwordTextItem.textFieldValue
                                      note:_noteTextItem.text];
}

// Called when the user taps the Suggest Strong Password button.
- (void)didTapSuggestStrongPassword:(UIButton*)sender {
  _passwordTextItem.textFieldSecureTextEntry = NO;
  __weak __typeof(self) weakSelf = self;
  [self.delegate requestGeneratedPasswordWithCompletion:^(NSString* password) {
    [weakSelf updatePasswordTextFieldValue:password];
  }];
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
    case ItemTypeSuggestPassword:
      return NO;
  };
}

#pragma mark - Private

- (BOOL)checkIfValidSite {
  BOOL siteEmpty = [_websiteTextItem.textFieldValue length] == 0;
  if (!siteEmpty && !_isTLDMissingMessageShown && !_isDuplicatedCredential) {
    _websiteTextItem.hasValidText = YES;
    [self reconfigureCellsForItems:@[ _websiteTextItem ]];
  }
  return !siteEmpty;
}

// Checks if the password is valid and updates item accordingly.
- (BOOL)checkIfValidPassword {
  BOOL passwordEmpty = [_passwordTextItem.textFieldValue length] == 0;
  if (!passwordEmpty) {
    _passwordTextItem.hasValidText = YES;
    [self reconfigureCellsForItems:@[ _passwordTextItem ]];
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
      !_isDuplicatedCredential && _shouldEnableSave &&
      [self.delegate isURLValid] && !_isTLDMissingMessageShown;
}

// Shows the section with the error message for top-level domain missing.
- (void)showTLDMissingSection {
  if (_isTLDMissingMessageShown) {
    return;
  }

  self.navigationItem.rightBarButtonItem.enabled = NO;
  _isTLDMissingMessageShown = YES;
  __weak __typeof(self) weakSelf = self;
  [self
      performBatchTableViewUpdates:^{
        [weakSelf updateTLDMissingSectionCompletion];
      }
                        completion:nil];
}

- (BOOL)isPasswordShown {
  return _passwordShown;
}

// Updates the table view to show the TLD message.
- (void)updateTLDMissingSectionCompletion {
  [self.tableViewModel setFooter:[self TLDMessageFooterItem]
        forSectionWithIdentifier:SectionIdentifierTLDFooter];
  NSUInteger index = [self.tableViewModel
      sectionForSectionIdentifier:SectionIdentifierTLDFooter];
  [self.tableView reloadSections:[NSIndexSet indexSetWithIndex:index]
                withRowAnimation:UITableViewRowAnimationNone];
}

// Updates the table view based on if a duplicate credentials was found.
- (void)updateTableViewWithDuplicatesFound:(BOOL)duplicateFound {
  if (duplicateFound) {
    TableViewModel* model = self.tableViewModel;
    NSUInteger passwordSectionIndex = [self.tableViewModel
        sectionForSectionIdentifier:SectionIdentifierPassword];
    [model insertSectionWithIdentifier:SectionIdentifierDuplicate
                               atIndex:passwordSectionIndex + 1];
    [self.tableView
          insertSections:[NSIndexSet indexSetWithIndex:passwordSectionIndex + 1]
        withRowAnimation:UITableViewRowAnimationTop];
    [model addItem:[self duplicatePasswordMessageItem]
        toSectionWithIdentifier:SectionIdentifierDuplicate];
    [model addItem:[self duplicatePasswordViewButtonItem]
        toSectionWithIdentifier:SectionIdentifierDuplicate];
    if (_usernameTextItem && [_usernameTextItem.textFieldValue length] > 0) {
      _usernameTextItem.hasValidText = NO;
      [self reconfigureCellsForItems:@[ _usernameTextItem ]];
      return;
    }
    _websiteTextItem.hasValidText = NO;
    [self reconfigureCellsForItems:@[ _websiteTextItem ]];
    return;
  }
  [self removeSectionWithIdentifier:SectionIdentifierDuplicate
                   withRowAnimation:UITableViewRowAnimationTop];
  _usernameTextItem.hasValidText = YES;
  _websiteTextItem.hasValidText = YES;
  [self reconfigureCellsForItems:@[ _websiteTextItem, _usernameTextItem ]];
}

// Updates the visibility of the note footer in the table view.
- (void)updateNoteFooterVisibility:(BOOL)shouldDisplayNoteFooter {
  [self.tableViewModel setFooter:shouldDisplayNoteFooter
                                     ? [self tooLongNoteMessageFooterItem]
                                     : nil
        forSectionWithIdentifier:SectionIdentifierNoteFooter];
  NSUInteger index = [self.tableViewModel
      sectionForSectionIdentifier:SectionIdentifierNoteFooter];
  [self.tableView reloadSections:[NSIndexSet indexSetWithIndex:index]
                withRowAnimation:UITableViewRowAnimationNone];
}

// Removes the footer associated with TLD.
- (void)removeTLDFooter {
  [self.tableViewModel setFooter:nil
        forSectionWithIdentifier:SectionIdentifierTLDFooter];
  NSUInteger index = [self.tableViewModel
      sectionForSectionIdentifier:SectionIdentifierTLDFooter];
  [self.tableView reloadSections:[NSIndexSet indexSetWithIndex:index]
                withRowAnimation:UITableViewRowAnimationNone];
}

#pragma mark - Actions

// Called when the user tapped on the show/hide button near password.
- (void)didTapShowHideButton:(UIButton*)buttonView {
  [self.tableView deselectRowAtIndexPath:self.tableView.indexPathForSelectedRow
                                animated:NO];
  if (_passwordShown) {
    _passwordShown = NO;
    _passwordTextItem.textFieldSecureTextEntry = YES;
    // Only change the textFieldValue for tests.
    if (_passwordForTesting) {
      _passwordTextItem.textFieldValue = kMaskedPassword;
    }
    _passwordTextItem.identifyingIcon =
        DefaultSymbolWithPointSize(kShowActionSymbol, kSymbolSize);
    _passwordTextItem.identifyingIconAccessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_SHOW_BUTTON);
    [self reconfigureCellsForItems:@[ _passwordTextItem ]];
  } else {
    _passwordTextItem.textFieldSecureTextEntry = NO;
    _passwordShown = YES;
    // Only change the textFieldValue for tests.
    if (_passwordForTesting) {
      _passwordTextItem.textFieldValue = _passwordForTesting;
    }
    _passwordTextItem.identifyingIcon =
        DefaultSymbolWithPointSize(kHideActionSymbol, kSymbolSize);
    _passwordTextItem.identifyingIconAccessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_HIDE_BUTTON);
    [self reconfigureCellsForItems:@[ _passwordTextItem ]];
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
      _usernameErrorAnchorView;
  errorInfoPopover.popoverPresentationController.sourceRect =
      _usernameErrorAnchorView.bounds;
  errorInfoPopover.popoverPresentationController.permittedArrowDirections =
      UIPopoverArrowDirectionAny;
  [self presentViewController:errorInfoPopover animated:YES completion:nil];
}

#pragma mark - ForTesting

- (void)setPassword:(NSString*)password {
  NSIndexPath* indexPath =
      [self.tableViewModel indexPathForItem:_passwordTextItem];
  TableViewTextEditItem* item = static_cast<TableViewTextEditItem*>(
      [self.tableViewModel itemAtIndexPath:indexPath]);
  _passwordForTesting = password;
  item.textFieldValue = [self isPasswordShown] || self.tableView.editing
                            ? _passwordForTesting
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
