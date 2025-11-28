// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/address_editor/autofill_profile_edit_table_view_helper.h"

#import "base/apple/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/field_types.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/autofill_constants.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/autofill_profile_edit_table_view_helper_delegate.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/cells/autofill_edit_profile_button_footer_item.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/cells/autofill_profile_edit_item.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/cells/country_item.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_credit_card_ui_type_util.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_attributed_string_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_multi_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// A constant to separate the error and the footer text.
const CGFloat kLineSpacingBetweenErrorAndFooter = 12.0f;

}  // namespace

@interface AutofillProfileEditTableViewHelper () <
    AutofillEditProfileButtonFooterDelegate>

// If YES, denotes that the view is laid out for the migration prompt.
@property(nonatomic, assign) BOOL migrationPrompt;

@end

@implementation AutofillProfileEditTableViewHelper {
  NSString* _userEmail;

  // The AutofillProfileEditTableViewHelperDelegate for this ViewController.
  __weak id<AutofillProfileEditTableViewHelperDelegate> _delegate;

  // The shown view controller.
  __weak LegacyChromeTableViewController* _controller;

  // The item for the save/update button. Used when the
  // `kAutofillDynamicallyLoadsFieldsForAddressInput` feature is disabled.
  TableViewTextButtonItem* _modalSaveUpdateButton;

  // The button footer item for the save/update button. Used when the
  // `kAutofillDynamicallyLoadsFieldsForAddressInput` feature is enabled.
  AutofillEditProfileButtonFooterItem* _saveUpdateButtonFooterItem;

  // If YES, the table view has a save button.
  BOOL _hasSaveButton;

  // If YES, the table view has an update button.
  BOOL _hasUpdateButton;

  // Denotes that the views are laid out to migrate an incomplete profile to
  // account from the settings.
  BOOL _moveToAccountFromSettings;

  // The specific context in which this address editor is being presented.
  SaveAddressContext _addressContext;

  // Stores the record type for the profile.
  autofill::AutofillProfile::RecordType _recordType;
}

#pragma mark - Initialization

- (instancetype)initWithDelegate:
                    (id<AutofillProfileEditTableViewHelperDelegate>)delegate
                       userEmail:(NSString*)userEmail
                      controller:(LegacyChromeTableViewController*)controller
                  addressContext:(SaveAddressContext)addressContext {
  self = [super init];
  if (self) {
    _delegate = delegate;
    _userEmail = userEmail;
    _controller = controller;
    _addressContext = addressContext;
    _moveToAccountFromSettings = NO;
    _hasSaveButton = NO;
    _hasUpdateButton = NO;
  }

  return self;
}

#pragma mark - AutofillProfileEditHandler

- (void)viewDidDisappear {
  [_delegate viewDidDisappear];
}

// Updates the profile via the delegate.
- (void)updateProfileData {
  const std::array<AutofillProfileDetailsSectionIdentifier, 3> allSections = {
      AutofillProfileDetailsSectionIdentifierName,
      AutofillProfileDetailsSectionIdentifierAddress,
      AutofillProfileDetailsSectionIdentifierPhoneEmail};

  for (const AutofillProfileDetailsSectionIdentifier section : allSections) {
    [self updateProfileDataForSection:section];
  }
}

- (void)reconfigureCells {
  const std::array<AutofillProfileDetailsSectionIdentifier, 3> allSections = {
      AutofillProfileDetailsSectionIdentifierName,
      AutofillProfileDetailsSectionIdentifierAddress,
      AutofillProfileDetailsSectionIdentifierPhoneEmail};

  for (const AutofillProfileDetailsSectionIdentifier section : allSections) {
    [_controller
        reconfigureCellsForItems:[_controller.tableViewModel
                                     itemsInSectionWithIdentifier:section]];
  }
}

- (void)loadModel {
  TableViewModel* model = _controller.tableViewModel;

  if (![self isHomeOrWorkProfile] ||
      _addressContext != SaveAddressContext::kEditingSavedAddress) {
    if (![model hasSectionForSectionIdentifier:
                    AutofillProfileDetailsSectionIdentifierName]) {
      [model
          addSectionWithIdentifier:AutofillProfileDetailsSectionIdentifierName];
    }
    for (AutofillEditProfileField* nonAddressField in
         [_delegate inputNonAddressFields]) {
      if ([self isNameAndEmailProfile] &&
          ![[_delegate fieldTypeToTypeName:autofill::FieldType::NAME_FULL]
              isEqualToString:nonAddressField.fieldType]) {
        continue;
      }
      [model addItem:[self profileEditItem:nonAddressField.fieldLabel
                                 fieldType:nonAddressField.fieldType]
          toSectionWithIdentifier:AutofillProfileDetailsSectionIdentifierName];
    }

    if ([self isNameAndEmailProfile]) {
      [model addItem:[self emailItem]
          toSectionWithIdentifier:AutofillProfileDetailsSectionIdentifierName];
      return;
    }
  }

  if (![model hasSectionForSectionIdentifier:
                  AutofillProfileDetailsSectionIdentifierAddress]) {
    [model addSectionWithIdentifier:
               AutofillProfileDetailsSectionIdentifierAddress];
  }
  for (AutofillEditProfileField* addressField in
       [_delegate inputAddressFields]) {
    [model addItem:[self profileEditItem:addressField.fieldLabel
                               fieldType:addressField.fieldType]
        toSectionWithIdentifier:AutofillProfileDetailsSectionIdentifierAddress];
  }
  [model addItem:[self countryItem]
      toSectionWithIdentifier:AutofillProfileDetailsSectionIdentifierAddress];

  if (![self isHomeOrWorkProfile] ||
      _addressContext != SaveAddressContext::kEditingSavedAddress) {
    if (![model hasSectionForSectionIdentifier:
                    AutofillProfileDetailsSectionIdentifierPhoneEmail]) {
      [model addSectionWithIdentifier:
                 AutofillProfileDetailsSectionIdentifierPhoneEmail];
    }
    [model addItem:[self phoneItem]
        toSectionWithIdentifier:
            AutofillProfileDetailsSectionIdentifierPhoneEmail];
    [model addItem:[self emailItem]
        toSectionWithIdentifier:
            AutofillProfileDetailsSectionIdentifierPhoneEmail];
  }
}

- (UITableViewCell*)cell:(UITableViewCell*)cell
       forRowAtIndexPath:(NSIndexPath*)indexPath
        withTextDelegate:(id<UITextFieldDelegate>)delegate {
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  NSInteger itemType =
      [_controller.tableViewModel itemTypeForIndexPath:indexPath];

  if (itemType == AutofillProfileDetailsItemTypeSaveButton) {
    TableViewTextButtonCell* tableViewTextButtonCell =
        base::apple::ObjCCastStrict<TableViewTextButtonCell>(cell);
    [tableViewTextButtonCell.button addTarget:self
                                       action:@selector(didTapSaveButton)
                             forControlEvents:UIControlEventTouchUpInside];
    return tableViewTextButtonCell;
  }

  if (itemType == AutofillProfileDetailsItemTypeCountrySelectionField) {
    if ([self showEditView]) {
      cell.selectionStyle = UITableViewCellSelectionStyleDefault;
    }
    return cell;
  }

  TableViewTextEditCell* textFieldCell =
      base::apple::ObjCCastStrict<TableViewTextEditCell>(cell);
  textFieldCell.accessibilityIdentifier = textFieldCell.textLabel.text;
  textFieldCell.textField.delegate = delegate;
  return textFieldCell;
}

- (void)didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger itemType =
      [_controller.tableViewModel itemTypeForIndexPath:indexPath];
  if ([self showEditView]) {
    if (itemType == AutofillProfileDetailsItemTypeCountrySelectionField) {
      [_delegate willSelectCountryWithCurrentlySelectedCountry:
                     [self countryFieldCurrentValue]];
    } else if ([self isItemTypeTextEditCell:itemType]) {
      UITableViewCell* cell =
          [_controller.tableView cellForRowAtIndexPath:indexPath];
      TableViewTextEditCell* textFieldCell =
          base::apple::ObjCCastStrict<TableViewTextEditCell>(cell);
      [textFieldCell.textField becomeFirstResponder];
    }
  }
}

- (void)configureView:(UIView*)view forFooterInSection:(NSInteger)section {
  NSInteger sectionIdentifier =
      [_controller.tableViewModel sectionIdentifierForSectionIndex:section];

  if (sectionIdentifier == AutofillProfileDetailsSectionIdentifierButton) {
    AutofillEditProfileButtonFooterCell* buttonView =
        base::apple::ObjCCast<AutofillEditProfileButtonFooterCell>(view);
    buttonView.delegate = self;
  }
}

- (BOOL)heightForHeaderShouldBeZeroInSection:(NSInteger)section {
  NSInteger sectionIdentifier =
      [_controller.tableViewModel sectionIdentifierForSectionIndex:section];

  return sectionIdentifier == AutofillProfileDetailsSectionIdentifierFooter ||
         sectionIdentifier ==
             AutofillProfileDetailsSectionIdentifierErrorFooter;
}

- (BOOL)heightForFooterShouldBeZeroInSection:(NSInteger)section {
  NSInteger sectionIdentifier =
      [_controller.tableViewModel sectionIdentifierForSectionIndex:section];

  if ([self isHomeOrWorkProfile] &&
      _addressContext == SaveAddressContext::kEditingSavedAddress) {
    return sectionIdentifier == AutofillProfileDetailsSectionIdentifierAddress;
  }
  return sectionIdentifier == AutofillProfileDetailsSectionIdentifierPhoneEmail;
}

- (void)loadFooterForSettings {
  CHECK(_addressContext == SaveAddressContext::kEditingSavedAddress);
  TableViewModel* model = _controller.tableViewModel;

  if ([self isAccountProfile] || [self isNameAndEmailProfile]) {
    CHECK(_userEmail);
    [model
        addSectionWithIdentifier:AutofillProfileDetailsSectionIdentifierFooter];
    [model setFooter:[self footerItem]
        forSectionWithIdentifier:AutofillProfileDetailsSectionIdentifierFooter];
  }
}

- (void)loadMessageAndButtonForModalIfSaveOrUpdate:(BOOL)update {
  CHECK(_addressContext != SaveAddressContext::kEditingSavedAddress);
  _hasSaveButton = !update;
  _hasUpdateButton = update;
  TableViewModel* model = _controller.tableViewModel;

  if ([self isAccountProfile] || self.migrationPrompt) {
    CHECK([_userEmail length] > 0);
    [model
        addSectionWithIdentifier:AutofillProfileDetailsSectionIdentifierFooter];
    [model setFooter:[self footerItem]
        forSectionWithIdentifier:AutofillProfileDetailsSectionIdentifierFooter];
  }

  [model
      addSectionWithIdentifier:AutofillProfileDetailsSectionIdentifierButton];
  [model setFooter:[self saveUpdateButtonAsFooter:update]
      forSectionWithIdentifier:AutofillProfileDetailsSectionIdentifierButton];
}

- (BOOL)isItemAtIndexPathTextEditCell:(NSIndexPath*)cellPath {
  NSInteger itemType =
      [_controller.tableViewModel itemTypeForIndexPath:cellPath];
  return [self isItemTypeTextEditCell:itemType];
}

- (void)setMoveToAccountFromSettings:(BOOL)moveToAccountFromSettings {
  if (_moveToAccountFromSettings == moveToAccountFromSettings) {
    return;
  }

  _moveToAccountFromSettings = moveToAccountFromSettings;
  if (moveToAccountFromSettings) {
    [self findRequiredFieldsWithEmptyValues];
  }
}

#pragma mark - TableViewTextEditItemDelegate

- (void)tableViewItemDidBeginEditing:(TableViewTextEditItem*)tableViewItem {
  [_controller reconfigureCellsForItems:@[ tableViewItem ]];
}

- (void)tableViewItemDidChange:(TableViewTextEditItem*)tableViewItem {
  AutofillProfileEditItem* profileItem =
      base::apple::ObjCCastStrict<AutofillProfileEditItem>(tableViewItem);
  [_delegate computeFieldWasEdited:profileItem.autofillFieldType
                             value:tableViewItem.textFieldValue];
  if (([self isAccountProfile] || self.migrationPrompt ||
       _moveToAccountFromSettings)) {
    tableViewItem.hasValidText = [_delegate
          fieldContainsValidValue:profileItem.autofillFieldType
                    hasEmptyValue:(profileItem.textFieldValue.length == 0)
        moveToAccountFromSettings:_moveToAccountFromSettings];
    [_delegate validateFieldsAndUpdateButtonStatus];
  }

  [_controller reconfigureCellsForItems:@[ tableViewItem ]];
}

- (void)tableViewItemDidEndEditing:(TableViewTextEditItem*)tableViewItem {
  [_controller reconfigureCellsForItems:@[ tableViewItem ]];

  // Record new value in current state.
  [_delegate setCurrentValueForType:((AutofillProfileEditItem*)tableViewItem)
                                        .autofillFieldType
                          withValue:tableViewItem.textFieldValue];
}

#pragma mark - AutofillProfileEditConsumer

- (void)didSelectCountry:(NSString*)country {
  // Remove the previously inserted fields.
  TableViewModel* model = _controller.tableViewModel;
  [model deleteAllItemsFromSectionWithIdentifier:
             AutofillProfileDetailsSectionIdentifierName];
  [model deleteAllItemsFromSectionWithIdentifier:
             AutofillProfileDetailsSectionIdentifierAddress];
  [model deleteAllItemsFromSectionWithIdentifier:
             AutofillProfileDetailsSectionIdentifierPhoneEmail];

  // Re-insert the fields based on the new country.
  [self loadModel];

  // Reload the table view with the new fields.
  [_controller.tableView reloadData];

  [_delegate setCurrentValueForType:[self countryFieldKeyValue]
                          withValue:country];
  [self findRequiredFieldsWithEmptyValues];
}

- (void)setProfileRecordType:(autofill::AutofillProfile::RecordType)recordType {
  _recordType = recordType;
}

- (void)updateErrorStatus:(BOOL)shouldShowError {
  AutofillProfileDetailsSectionIdentifier addSection =
      shouldShowError ? AutofillProfileDetailsSectionIdentifierErrorFooter
                      : AutofillProfileDetailsSectionIdentifierFooter;
  AutofillProfileDetailsSectionIdentifier removeSection =
      shouldShowError ? AutofillProfileDetailsSectionIdentifierFooter
                      : AutofillProfileDetailsSectionIdentifierErrorFooter;

  [self changeFooterStatusToRemoveSection:removeSection addSection:addSection];
}

- (void)updateErrorMessageIfRequired {
  if ([self shouldChangeErrorMessage]) {
    [self
        changeFooterStatusToRemoveSection:
            AutofillProfileDetailsSectionIdentifierErrorFooter
                               addSection:
                                   AutofillProfileDetailsSectionIdentifierErrorFooter];
  }
}

- (void)updateButtonStatus:(BOOL)enabled {
  if (_addressContext == SaveAddressContext::kEditingSavedAddress) {
    _controller.navigationItem.rightBarButtonItem.enabled = enabled;
  } else {
    _saveUpdateButtonFooterItem.enabled = enabled;

    NSInteger section = [[_controller tableViewModel]
        sectionForSectionIdentifier:
            AutofillProfileDetailsSectionIdentifierButton];
    UITableViewHeaderFooterView* footerView =
        [_controller.tableView footerViewForSection:section];

    if (footerView) {
      [_saveUpdateButtonFooterItem
          configureHeaderFooterView:footerView
                         withStyler:_controller.styler];
    }
  }
}

#pragma mark - AutofillEditProfileButtonFooterDelegate

- (void)didTapButton {
  CHECK(_addressContext != SaveAddressContext::kEditingSavedAddress);
  if (_addressContext == SaveAddressContext::kAddingManualAddress) {
    base::RecordAction(
        base::UserMetricsAction("AddAddressManually_AddressSaved"));
  } else if (_hasSaveButton) {
    base::RecordAction(base::UserMetricsAction("AddressInfobar_AddressSaved"));
  }
  [_delegate didSaveProfileFromModal];
}

#pragma mark - Actions

- (void)didTapSaveButton {
  CHECK(_addressContext != SaveAddressContext::kEditingSavedAddress);
  [_delegate didSaveProfileFromModal];
}

#pragma mark - Items

// Creates and returns the `TableViewLinkHeaderFooterItem` footer item.
- (TableViewLinkHeaderFooterItem*)footerItem {
  TableViewLinkHeaderFooterItem* item = [[TableViewLinkHeaderFooterItem alloc]
      initWithType:AutofillProfileDetailsItemTypeFooter];
  item.text = [self footerMessage];
  return item;
}

// Returns the error message item as a footer.
- (TableViewAttributedStringHeaderFooterItem*)errorMessageItem {
  TableViewAttributedStringHeaderFooterItem* item =
      [[TableViewAttributedStringHeaderFooterItem alloc]
          initWithType:AutofillProfileDetailsItemTypeError];
  [item setAttributedString:[self errorAndFooterMessage]];
  return item;
}

// Returns the phone field used in the save/update prompts as well as the
// settings view.
- (AutofillProfileEditItem*)phoneItem {
  return [self
      autofillEditItemFromAutofillType:
          [_delegate fieldTypeToTypeName:autofill::PHONE_HOME_WHOLE_NUMBER]
                            fieldLabel:l10n_util::GetNSString(
                                           IDS_IOS_AUTOFILL_PHONE)
                         returnKeyType:UIReturnKeyNext
                          keyboardType:UIKeyboardTypePhonePad
                autoCapitalizationType:UITextAutocapitalizationTypeSentences];
}

// Returns the email field used in the save/update prompts as well as the
// settings view.
- (AutofillProfileEditItem*)emailItem {
  return
      [self autofillEditItemFromAutofillType:
                [_delegate fieldTypeToTypeName:autofill::EMAIL_ADDRESS]
                                  fieldLabel:l10n_util::GetNSString(
                                                 IDS_IOS_AUTOFILL_EMAIL)
                               returnKeyType:UIReturnKeyDone
                                keyboardType:UIKeyboardTypeEmailAddress
                      autoCapitalizationType:UITextAutocapitalizationTypeNone];
}

// Returns the address field used in the save/update prompts as well as the
// settings view.
- (AutofillProfileEditItem*)profileEditItem:(NSString*)fieldLabel
                                  fieldType:(NSString*)fieldType {
  return [self
      autofillEditItemFromAutofillType:fieldType
                            fieldLabel:fieldLabel
                         returnKeyType:UIReturnKeyNext
                          keyboardType:UIKeyboardTypeDefault
                autoCapitalizationType:UITextAutocapitalizationTypeSentences];
}

// Returns autofill text field of type `AutofillProfileEditItem` used in
// save/update prompts as well as the settings view.
- (AutofillProfileEditItem*)
    autofillEditItemFromAutofillType:(NSString*)autofillType
                          fieldLabel:(NSString*)fieldLabel
                       returnKeyType:(UIReturnKeyType)returnKeyType
                        keyboardType:(UIKeyboardType)keyboardType
              autoCapitalizationType:
                  (UITextAutocapitalizationType)autoCapitalizationType {
  AutofillProfileEditItem* item = [[AutofillProfileEditItem alloc]
      initWithType:AutofillProfileDetailsItemTypeTextField];
  item.fieldNameLabelText = fieldLabel;
  item.autofillFieldType = autofillType;
  item.textFieldValue = [_delegate currentValueForType:item.autofillFieldType];
  item.textFieldEnabled = [self showEditView];
  item.hideIcon =
      (_addressContext != SaveAddressContext::kEditingSavedAddress) ||
      ![self showEditView];
  item.autoCapitalizationType = autoCapitalizationType;
  item.returnKeyType =
      _addressContext == SaveAddressContext::kEditingSavedAddress
          ? returnKeyType
          : UIReturnKeyDone;
  item.keyboardType = keyboardType;
  item.delegate = self;
  item.accessibilityIdentifier = item.fieldNameLabelText;
  item.useCustomSeparator =
      _addressContext != SaveAddressContext::kEditingSavedAddress;
  return item;
}

// Returns the country field used in the save/update prompts as well as the
// settings view.
- (TableViewMultiDetailTextItem*)countryItem {
  TableViewMultiDetailTextItem* item = [[TableViewMultiDetailTextItem alloc]
      initWithType:AutofillProfileDetailsItemTypeCountrySelectionField];
  item.text = l10n_util::GetNSString(IDS_IOS_AUTOFILL_COUNTRY);
  item.trailingDetailText = [self countryFieldCurrentValue];
  item.trailingDetailTextColor = [UIColor colorNamed:kTextPrimaryColor];
  item.useCustomSeparator =
      _addressContext != SaveAddressContext::kEditingSavedAddress;
  if (_addressContext != SaveAddressContext::kEditingSavedAddress) {
    item.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  } else if (_controller.tableView.editing) {
    item.editingAccessoryType = UITableViewCellAccessoryDisclosureIndicator;
  }
  return item;
}

// Returns the button element for the save/update prompts.
- (TableViewTextButtonItem*)saveButtonIfSaveOrUpdate:(BOOL)update {
  CHECK(_addressContext != SaveAddressContext::kEditingSavedAddress);
  _modalSaveUpdateButton = [[TableViewTextButtonItem alloc]
      initWithType:AutofillProfileDetailsItemTypeSaveButton];
  if (self.migrationPrompt) {
    _modalSaveUpdateButton.buttonText = l10n_util::GetNSString(
        IDS_AUTOFILL_ADDRESS_MIGRATION_TO_ACCOUNT_PROMPT_OK_BUTTON_LABEL);
  } else {
    _modalSaveUpdateButton.buttonText = l10n_util::GetNSString(
        update ? IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_OK_BUTTON_LABEL
               : IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_OK_BUTTON_LABEL);
  }

  _modalSaveUpdateButton.disableButtonIntrinsicWidth = YES;

  return _modalSaveUpdateButton;
}

- (AutofillEditProfileButtonFooterItem*)saveUpdateButtonAsFooter:(BOOL)update {
  CHECK(_addressContext != SaveAddressContext::kEditingSavedAddress);
  _saveUpdateButtonFooterItem = [[AutofillEditProfileButtonFooterItem alloc]
      initWithType:AutofillProfileDetailsItemTypeSaveButton];
  if (self.migrationPrompt) {
    _saveUpdateButtonFooterItem.buttonText = l10n_util::GetNSString(
        IDS_AUTOFILL_ADDRESS_MIGRATION_TO_ACCOUNT_PROMPT_OK_BUTTON_LABEL);
  } else {
    _saveUpdateButtonFooterItem.buttonText = l10n_util::GetNSString(
        update ? IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_OK_BUTTON_LABEL
               : IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_OK_BUTTON_LABEL);
  }
  // The button should initially be disabled when manually adding a new address
  // to the account.
  _saveUpdateButtonFooterItem.enabled =
      _addressContext != SaveAddressContext::kAddingManualAddress ||
      ![self isAccountProfile];
  return _saveUpdateButtonFooterItem;
}

#pragma mark - Private

// Removes the given section if it exists.
- (void)removeSectionWithIdentifier:(NSInteger)sectionIdentifier
                   withRowAnimation:(UITableViewRowAnimation)animation {
  TableViewModel* model = _controller.tableViewModel;
  if ([model hasSectionForSectionIdentifier:sectionIdentifier]) {
    NSInteger section = [model sectionForSectionIdentifier:sectionIdentifier];
    [model removeSectionWithIdentifier:sectionIdentifier];
    [[_controller tableView]
          deleteSections:[NSIndexSet indexSetWithIndex:section]
        withRowAnimation:animation];
  }
}

// If the error status has changed, displays the footer accordingly.
- (void)changeFooterStatusToRemoveSection:
            (AutofillProfileDetailsSectionIdentifier)removeSection
                               addSection:
                                   (AutofillProfileDetailsSectionIdentifier)
                                       addSection {
  TableViewModel* model = _controller.tableViewModel;
  __weak AutofillProfileEditTableViewHelper* weakSelf = self;
  [_controller
      performBatchTableViewUpdates:^{
        AutofillProfileEditTableViewHelper* strongSelf = weakSelf;

        if (!strongSelf) {
          return;
        }

        [strongSelf removeSectionWithIdentifier:removeSection
                               withRowAnimation:UITableViewRowAnimationTop];

        AutofillProfileDetailsSectionIdentifier lastFieldSection =
            AutofillProfileDetailsSectionIdentifierPhoneEmail;
        NSUInteger fieldsSectionIndex =
            [model sectionForSectionIdentifier:lastFieldSection];
        [model insertSectionWithIdentifier:addSection
                                   atIndex:fieldsSectionIndex + 1];
        [strongSelf->_controller.tableView
              insertSections:[NSIndexSet
                                 indexSetWithIndex:fieldsSectionIndex + 1]
            withRowAnimation:UITableViewRowAnimationTop];
        [strongSelf->_controller.tableViewModel
                           setFooter:
                               (([strongSelf->_delegate
                                         requiredFieldsWithEmptyValuesCount] >
                                 0)
                                    ? [strongSelf errorMessageItem]
                                    : [strongSelf footerItem])
            forSectionWithIdentifier:addSection];
      }
                        completion:nil];
}

// Returns YES, if the error message needs to be changed. This happens when
// there are multiple required fields that become empty.
- (BOOL)shouldChangeErrorMessage {
  TableViewHeaderFooterItem* currentFooter = [_controller.tableViewModel
      footerForSectionWithIdentifier:
          AutofillProfileDetailsSectionIdentifierErrorFooter];
  TableViewAttributedStringHeaderFooterItem* attributedFooterItem =
      base::apple::ObjCCastStrict<TableViewAttributedStringHeaderFooterItem>(
          currentFooter);
  NSAttributedString* newFooter = [self errorAndFooterMessage];
  return ![attributedFooterItem.attributedString
      isEqualToAttributedString:newFooter];
}

// Returns YES, if the view controller is shown in the edit mode.
- (BOOL)showEditView {
  return _addressContext != SaveAddressContext::kEditingSavedAddress ||
         _controller.tableView.editing;
}

// Returns the footer message.
- (NSString*)footerMessage {
  CHECK([_userEmail length] > 0);
  if (([self isHomeOrWorkProfile] || [self isNameAndEmailProfile]) &&
      _addressContext == SaveAddressContext::kEditingSavedAddress) {
    return l10n_util::GetNSStringF(IDS_IOS_AUTOFILL_HOME_WORK_PROFILE_FOOTER,
                                   base::SysNSStringToUTF16(_userEmail));
  }
  return _moveToAccountFromSettings
             ? @""
             : l10n_util::GetNSStringF(
                   _hasSaveButton
                       ? IDS_IOS_AUTOFILL_SAVE_ADDRESS_IN_ACCOUNT_FOOTER
                       : IDS_IOS_SETTINGS_AUTOFILL_ACCOUNT_ADDRESS_FOOTER_TEXT,
                   base::SysNSStringToUTF16(_userEmail));
}

// Returns the error message combined with footer.
- (NSAttributedString*)errorAndFooterMessage {
  CHECK([_delegate requiredFieldsWithEmptyValuesCount] > 0);
  NSString* error = l10n_util::GetPluralNSStringF(
      IDS_IOS_SETTINGS_EDIT_AUTOFILL_ADDRESS_REQUIREMENT_ERROR,
      [_delegate requiredFieldsWithEmptyValuesCount]);

  NSString* finalErrorString =
      [NSString stringWithFormat:@"%@\n%@", error, [self footerMessage]];

  NSMutableParagraphStyle* paragraphStyle =
      [[NSMutableParagraphStyle alloc] init];
  paragraphStyle.paragraphSpacing = kLineSpacingBetweenErrorAndFooter;

  NSMutableAttributedString* attributedText = [[NSMutableAttributedString alloc]
      initWithString:finalErrorString
          attributes:@{
            NSFontAttributeName :
                [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote],
            NSForegroundColorAttributeName :
                [UIColor colorNamed:kTextSecondaryColor],
            NSParagraphStyleAttributeName : paragraphStyle
          }];
  [attributedText addAttribute:NSForegroundColorAttributeName
                         value:[UIColor colorNamed:kRedColor]
                         range:NSMakeRange(0, error.length)];
  return attributedText;
}

// Returns YES if the `itemType` belongs to a text edit field.
- (BOOL)isItemTypeTextEditCell:(NSInteger)itemType {
  switch (static_cast<AutofillProfileDetailsItemType>(itemType)) {
    case AutofillProfileDetailsItemTypeTextField:
      return YES;
    case AutofillProfileDetailsItemTypeCountrySelectionField:
    case AutofillProfileDetailsItemTypeError:
    case AutofillProfileDetailsItemTypeFooter:
    case AutofillProfileDetailsItemTypeSaveButton:
    case AutofillProfileDetailsItemTypeMigrateToAccountButton:
    case AutofillProfileDetailsItemTypeMigrateToAccountRecommendation:
    case AutofillProfileDetailsItemTypeEdit:
      break;
  }
  return NO;
}

// Recomputes the required fields that are empty.
- (void)findRequiredFieldsWithEmptyValues {
  [_delegate resetRequiredFieldsWithEmptyValuesCount];
  AutofillProfileDetailsSectionIdentifier sectionIdentifier =
      AutofillProfileDetailsSectionIdentifierAddress;
  for (TableViewItem* item in [_controller.tableViewModel
           itemsInSectionWithIdentifier:sectionIdentifier]) {
    if (item.type == AutofillProfileDetailsItemTypeCountrySelectionField) {
      TableViewMultiDetailTextItem* multiDetailTextItem =
          base::apple::ObjCCastStrict<TableViewMultiDetailTextItem>(item);
      multiDetailTextItem.trailingDetailText = [self countryFieldCurrentValue];
    } else if ([self isItemTypeTextEditCell:item.type]) {
      // No requirement checks for local profiles.
      if ([self isAccountProfile] || self.migrationPrompt ||
          _moveToAccountFromSettings) {
        AutofillProfileEditItem* profileItem =
            base::apple::ObjCCastStrict<AutofillProfileEditItem>(item);
        profileItem.hasValidText = [_delegate
              fieldContainsValidValue:profileItem.autofillFieldType
                        hasEmptyValue:(profileItem.textFieldValue.length == 0)
            moveToAccountFromSettings:_moveToAccountFromSettings];
      }
    }
  }

  [_delegate validateFieldsAndUpdateButtonStatus];
  [self reconfigureCells];
}

// Returns the currently selected country value.
- (NSString*)countryFieldCurrentValue {
  return [_delegate currentValueForType:[self countryFieldKeyValue]];
}

// Returns the key for the country field.
- (NSString*)countryFieldKeyValue {
  return base::SysUTF8ToNSString(
      autofill::FieldTypeToString(autofill::ADDRESS_HOME_COUNTRY));
}

// Informs the delegate to update the field data for the section
// `sectionIdentifier`.
- (void)updateProfileDataForSection:
    (AutofillProfileDetailsSectionIdentifier)sectionIdentifier {
  TableViewModel* model = _controller.tableViewModel;
  NSInteger itemCount =
      [model numberOfItemsInSection:
                 [model sectionForSectionIdentifier:sectionIdentifier]];
  // Reads the values from the fields and updates the local copy of the
  // profile accordingly.
  NSInteger section = [model sectionForSectionIdentifier:sectionIdentifier];
  for (NSInteger itemIndex = 0; itemIndex < itemCount; ++itemIndex) {
    NSIndexPath* path = [NSIndexPath indexPathForItem:itemIndex
                                            inSection:section];
    NSInteger itemType = [_controller.tableViewModel itemTypeForIndexPath:path];

    if (itemType == AutofillProfileDetailsItemTypeCountrySelectionField) {
      [_delegate updateProfileMetadataWithValue:[self countryFieldCurrentValue]
                           forAutofillFieldType:[self countryFieldKeyValue]];
      continue;
    } else if (![self isItemTypeTextEditCell:itemType]) {
      continue;
    }

    AutofillProfileEditItem* item =
        base::apple::ObjCCastStrict<AutofillProfileEditItem>(
            [model itemAtIndexPath:path]);
    [_delegate updateProfileMetadataWithValue:item.textFieldValue
                         forAutofillFieldType:item.autofillFieldType];
  }
}

- (BOOL)isAccountProfile {
  return _recordType == autofill::AutofillProfile::RecordType::kAccount;
}

- (BOOL)isHomeOrWorkProfile {
  return _recordType == autofill::AutofillProfile::RecordType::kAccountHome ||
         _recordType == autofill::AutofillProfile::RecordType::kAccountWork;
}

- (BOOL)isNameAndEmailProfile {
  return _recordType ==
         autofill::AutofillProfile::RecordType::kAccountNameEmail;
}

@end
