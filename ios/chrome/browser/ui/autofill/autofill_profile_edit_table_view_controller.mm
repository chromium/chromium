// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/autofill_profile_edit_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_model/autofill_profile.h"
#import "components/autofill/core/browser/field_types.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_attributed_string_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_multi_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/autofill/autofill_profile_edit_table_view_constants.h"
#import "ios/chrome/browser/ui/autofill/autofill_profile_edit_table_view_controller_delegate.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type_util.h"
#import "ios/chrome/browser/ui/autofill/cells/autofill_profile_edit_item.h"
#import "ios/chrome/browser/ui/autofill/cells/country_item.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// A constant to separate the error and the footer text.
const CGFloat kLineSpacingBetweenErrorAndFooter = 12.0f;

}  // namespace

@interface AutofillProfileEditTableViewController ()

// Stores the value displayed in the fields.
@property(nonatomic, strong)
    NSMutableDictionary<NSString*, NSString*>* fieldValuesMap;

// YES, if the profile's source is autofill::AutofillProfile::Source::kAccount.
@property(nonatomic, assign) BOOL accountProfile;

// If YES, denotes that the view is laid out for the migration prompt.
@property(nonatomic, assign) BOOL migrationPrompt;

@end

@implementation AutofillProfileEditTableViewController {
  NSString* _userEmail;

  // The AutofillProfileEditTableViewControllerDelegate for this ViewController.
  __weak id<AutofillProfileEditTableViewControllerDelegate> _delegate;

  // Yes, if the error section has been presented.
  BOOL _errorSectionPresented;

  // The shown view controller.
  __weak LegacyChromeTableViewController* _controller;

  // If YES, denotes that the view is shown in the settings.
  BOOL _settingsView;

  // Points to the save/update button in the modal view.
  TableViewTextButtonItem* _modalSaveUpdateButton;

  // If YES, the table view has a save button.
  BOOL _hasSaveButton;

  // If YES, the table view has an update button.
  BOOL _hasUpdateButton;

  // Denotes that the views are laid out to migrate an incomplete profile to
  // account from the settings.
  BOOL _moveToAccountFromSettings;
}

#pragma mark - Initialization

- (instancetype)initWithDelegate:
                    (id<AutofillProfileEditTableViewControllerDelegate>)delegate
                       userEmail:(NSString*)userEmail
                      controller:(LegacyChromeTableViewController*)controller
                    settingsView:(BOOL)settingsView {
  self = [super init];
  if (self) {
    _delegate = delegate;
    _userEmail = userEmail;
    _errorSectionPresented = NO;
    _accountProfile = NO;
    _controller = controller;
    _settingsView = settingsView;
    _moveToAccountFromSettings = NO;
  }

  return self;
}

#pragma mark - AutofillProfileEditHandler

- (void)viewDidDisappear {
  [_delegate viewDidDisappear];
}

- (void)updateProfileData {
  TableViewModel* model = _controller.tableViewModel;
  NSInteger itemCount =
      [model numberOfItemsInSection:
                 [model sectionForSectionIdentifier:
                            AutofillProfileDetailsSectionIdentifierFields]];

  // Reads the values from the fields and updates the local copy of the
  // profile accordingly.
  NSInteger section = [model sectionForSectionIdentifier:
                                 AutofillProfileDetailsSectionIdentifierFields];
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

- (void)reconfigureCells {
  [_controller reconfigureCellsForItems:
                   [_controller.tableViewModel
                       itemsInSectionWithIdentifier:
                           AutofillProfileDetailsSectionIdentifierFields]];
}

- (void)loadModel {
  _hasSaveButton = NO;
  _hasUpdateButton = NO;

  TableViewModel* model = _controller.tableViewModel;

  NSString* countryCode = [_delegate selectedCountryCode];

  [model
      addSectionWithIdentifier:AutofillProfileDetailsSectionIdentifierFields];
  for (size_t i = 0; i < std::size(kProfileFieldsToDisplay); ++i) {
    const AutofillProfileFieldDisplayInfo& field = kProfileFieldsToDisplay[i];

    if (!FieldIsUsedInAddress(field.autofillType, countryCode)) {
      continue;
    }

    if (field.autofillType == autofill::ADDRESS_HOME_COUNTRY) {
      [model addItem:[self countryItem]
          toSectionWithIdentifier:
              AutofillProfileDetailsSectionIdentifierFields];
    } else {
      [model addItem:[self autofillEditItemFromField:field]
          toSectionWithIdentifier:
              AutofillProfileDetailsSectionIdentifierFields];
    }
  }
}

- (UITableViewCell*)cell:(UITableViewCell*)cell
       forRowAtIndexPath:(NSIndexPath*)indexPath
        withTextDelegate:(id<UITextFieldDelegate>)delegate {
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  NSInteger itemType =
      [_controller.tableViewModel itemTypeForIndexPath:indexPath];
  if (itemType == AutofillProfileDetailsItemTypeFooter ||
      itemType == AutofillProfileDetailsItemTypeError) {
    if (!_settingsView) {
      cell.separatorInset =
          UIEdgeInsetsMake(0, _controller.tableView.bounds.size.width, 0, 0);
    }
    return cell;
  }

  if (itemType == AutofillProfileDetailsItemTypeSaveButton) {
    TableViewTextButtonCell* tableViewTextButtonCell =
        base::apple::ObjCCastStrict<TableViewTextButtonCell>(cell);
    [tableViewTextButtonCell.button addTarget:self
                                       action:@selector(didTapSaveButton)
                             forControlEvents:UIControlEventTouchUpInside];
    return tableViewTextButtonCell;
  }

  if (itemType == AutofillProfileDetailsItemTypeCountrySelectionField) {
    TableViewMultiDetailTextCell* multiDetailTextCell =
        base::apple::ObjCCastStrict<TableViewMultiDetailTextCell>(cell);
    multiDetailTextCell.accessibilityIdentifier =
        multiDetailTextCell.textLabel.text;
    return multiDetailTextCell;
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
    } else if (itemType != AutofillProfileDetailsItemTypeFooter &&
               itemType != AutofillProfileDetailsItemTypeError &&
               [self isItemTypeTextEditCell:itemType]) {
      UITableViewCell* cell =
          [_controller.tableView cellForRowAtIndexPath:indexPath];
      TableViewTextEditCell* textFieldCell =
          base::apple::ObjCCastStrict<TableViewTextEditCell>(cell);
      [textFieldCell.textField becomeFirstResponder];
    }
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

  return (sectionIdentifier == AutofillProfileDetailsSectionIdentifierFields) ||
         (!_settingsView &&
          sectionIdentifier == AutofillProfileDetailsSectionIdentifierFooter);
}

- (void)loadFooterForSettings {
  CHECK(_settingsView);
  TableViewModel* model = _controller.tableViewModel;

  if (self.accountProfile && _userEmail != nil) {
    [model
        addSectionWithIdentifier:AutofillProfileDetailsSectionIdentifierFooter];
    [model setFooter:[self footerItem]
        forSectionWithIdentifier:AutofillProfileDetailsSectionIdentifierFooter];
  }
}

- (void)loadMessageAndButtonForModalIfSaveOrUpdate:(BOOL)update {
  CHECK(!_settingsView);
  TableViewModel* model = _controller.tableViewModel;
  if (self.accountProfile || self.migrationPrompt) {
    DCHECK([_userEmail length] > 0);
    [model addItem:[self footerItemForModalViewIfSaveOrUpdate:update]
        toSectionWithIdentifier:AutofillProfileDetailsSectionIdentifierFields];
  }

  [model addItem:[self saveButtonIfSaveOrUpdate:update]
      toSectionWithIdentifier:AutofillProfileDetailsSectionIdentifierFields];
  _hasSaveButton = !update;
  _hasUpdateButton = update;
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
  if ((self.accountProfile || self.migrationPrompt ||
       _moveToAccountFromSettings)) {
    [self computeErrorIfRequiredTextField:tableViewItem];
    if (_settingsView) {
      [self updateDoneButtonStatus];
    } else {
      [self updateSaveButtonStatus];
    }
  }
  [_controller reconfigureCellsForItems:@[ tableViewItem ]];
}

- (void)tableViewItemDidEndEditing:(TableViewTextEditItem*)tableViewItem {
  [_controller reconfigureCellsForItems:@[ tableViewItem ]];

  // Record new value in current state.
  self.fieldValuesMap[((AutofillProfileEditItem*)tableViewItem)
                          .autofillFieldType] = tableViewItem.textFieldValue;
}

#pragma mark - AutofillProfileEditConsumer

- (void)didSelectCountry:(NSString*)country {
  // Remove the previously inserted fields.
  TableViewModel* model = _controller.tableViewModel;
  [model removeSectionWithIdentifier:
             AutofillProfileDetailsSectionIdentifierFields];

  // Re-insert the fields based on the new country.
  BOOL hasButton = _hasSaveButton || _hasUpdateButton;
  BOOL update = _hasUpdateButton;
  [self loadModel];
  if (hasButton) {
    [self loadMessageAndButtonForModalIfSaveOrUpdate:update];
  }

  // Reload the table view with the new fields.
  [_controller.tableView reloadData];

  self.fieldValuesMap[[self countryFieldKeyValue]] = country;
  [self findRequiredFieldsWithEmptyValues];
}

#pragma mark - Actions

- (void)didTapSaveButton {
  CHECK(!_settingsView);
  [self updateProfileData];
  [_delegate didSaveProfileFromModal];
}

#pragma mark - Items

// Creates and returns the `TableViewLinkHeaderFooterItem` footer item.
- (TableViewLinkHeaderFooterItem*)footerItem {
  CHECK(_settingsView);
  TableViewLinkHeaderFooterItem* item = [[TableViewLinkHeaderFooterItem alloc]
      initWithType:AutofillProfileDetailsItemTypeFooter];
  item.text = [self footerMessage];
  return item;
}

// Returns the error message item as a footer.
- (TableViewAttributedStringHeaderFooterItem*)errorMessageItem {
  CHECK(_settingsView);
  TableViewAttributedStringHeaderFooterItem* item =
      [[TableViewAttributedStringHeaderFooterItem alloc]
          initWithType:AutofillProfileDetailsItemTypeError];
  [item setAttributedString:[self errorAndFooterMessage]];
  return item;
}

// Returns text fields displaying autofill field info used in save/update
// prompts as well as the settings view.
- (AutofillProfileEditItem*)autofillEditItemFromField:
    (const AutofillProfileFieldDisplayInfo&)field {
  AutofillProfileEditItem* item = [[AutofillProfileEditItem alloc]
      initWithType:AutofillProfileDetailsItemTypeTextField];
  item.fieldNameLabelText = l10n_util::GetNSString(field.displayStringID);
  item.autofillFieldType =
      base::SysUTF8ToNSString(autofill::FieldTypeToString(field.autofillType));
  item.textFieldValue = self.fieldValuesMap[item.autofillFieldType];
  item.textFieldEnabled = [self showEditView];
  item.hideIcon = ![self showEditView];
  item.autoCapitalizationType = field.autoCapitalizationType;
  item.returnKeyType = _settingsView ? field.returnKeyType : UIReturnKeyDone;
  item.keyboardType = field.keyboardType;
  item.delegate = self;
  item.accessibilityIdentifier = l10n_util::GetNSString(field.displayStringID);
  item.useCustomSeparator = !_settingsView;
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
  item.useCustomSeparator = !_settingsView;
  if (!_settingsView) {
    item.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  } else if (_controller.tableView.editing) {
    item.editingAccessoryType = UITableViewCellAccessoryDisclosureIndicator;
  }
  return item;
}

// Returns the footer element for the save/update prompts.
- (TableViewTextItem*)footerItemForModalViewIfSaveOrUpdate:(BOOL)update {
  CHECK(!_settingsView);
  TableViewTextItem* item = [[TableViewTextItem alloc]
      initWithType:AutofillProfileDetailsItemTypeFooter];
  item.text = l10n_util::GetNSStringF(
      update ? IDS_IOS_SETTINGS_AUTOFILL_ACCOUNT_ADDRESS_FOOTER_TEXT
             : IDS_IOS_AUTOFILL_SAVE_ADDRESS_IN_ACCOUNT_FOOTER,
      base::SysNSStringToUTF16(_userEmail));
  item.textFont = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  item.textColor = [UIColor colorNamed:kTextSecondaryColor];
  return item;
}

// Returns the button element for the save/update prompts.
- (TableViewTextButtonItem*)saveButtonIfSaveOrUpdate:(BOOL)update {
  CHECK(!_settingsView);
  _modalSaveUpdateButton = [[TableViewTextButtonItem alloc]
      initWithType:AutofillProfileDetailsItemTypeSaveButton];
  _modalSaveUpdateButton.textAlignment = NSTextAlignmentNatural;
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

#pragma mark - Private

// Computes whether the `tableViewItem` is a required field and empty.
- (void)computeErrorIfRequiredTextField:(TableViewTextEditItem*)tableViewItem {
  AutofillProfileEditItem* profileItem =
      static_cast<AutofillProfileEditItem*>(tableViewItem);
  tableViewItem.hasValidText = [_delegate
        fieldContainsValidValue:profileItem.autofillFieldType
                  hasEmptyValue:(profileItem.textFieldValue.length == 0)
      moveToAccountFromSettings:_moveToAccountFromSettings];
}

// Removes the given section if it exists.
- (void)removeSectionWithIdentifier:(NSInteger)sectionIdentifier
                   withRowAnimation:(UITableViewRowAnimation)animation {
  CHECK(_settingsView);
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
  CHECK(_settingsView);
  TableViewModel* model = _controller.tableViewModel;
  __weak AutofillProfileEditTableViewController* weakSelf = self;
  [_controller
      performBatchTableViewUpdates:^{
        AutofillProfileEditTableViewController* strongSelf = weakSelf;

        if (!strongSelf) {
          return;
        }
        [weakSelf removeSectionWithIdentifier:removeSection
                             withRowAnimation:UITableViewRowAnimationTop];
        NSUInteger fieldsSectionIndex =
            [model sectionForSectionIdentifier:
                       AutofillProfileDetailsSectionIdentifierFields];
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

// Updates the Done button status based on the required fields that are empty
// and shows/removes the error footer if required.
- (void)updateDoneButtonStatus {
  CHECK(_settingsView);
  BOOL shouldShowError = ([_delegate requiredFieldsWithEmptyValuesCount] > 0);
  _controller.navigationItem.rightBarButtonItem.enabled = !shouldShowError;
  if (shouldShowError != _errorSectionPresented) {
    AutofillProfileDetailsSectionIdentifier addSection =
        shouldShowError ? AutofillProfileDetailsSectionIdentifierErrorFooter
                        : AutofillProfileDetailsSectionIdentifierFooter;
    AutofillProfileDetailsSectionIdentifier removeSection =
        shouldShowError ? AutofillProfileDetailsSectionIdentifierFooter
                        : AutofillProfileDetailsSectionIdentifierErrorFooter;
    [self changeFooterStatusToRemoveSection:removeSection
                                 addSection:addSection];
    _errorSectionPresented = shouldShowError;
  } else if (shouldShowError && [self shouldChangeErrorMessage]) {
    [self
        changeFooterStatusToRemoveSection:
            AutofillProfileDetailsSectionIdentifierErrorFooter
                               addSection:
                                   AutofillProfileDetailsSectionIdentifierErrorFooter];
  }
}

// Updates the Save/Update button status based on the required fields that are
// empty.
- (void)updateSaveButtonStatus {
  CHECK(!_settingsView);
  BOOL shouldShowError = ([_delegate requiredFieldsWithEmptyValuesCount] > 0);
  _modalSaveUpdateButton.enabled = !shouldShowError;
  [_controller reconfigureCellsForItems:@[ _modalSaveUpdateButton ]];
}

// Returns YES, if the error message needs to be changed. This happens when
// there are multiple required fields that become empty.
- (BOOL)shouldChangeErrorMessage {
  CHECK(_settingsView);
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
  return !_settingsView || _controller.tableView.editing;
}

// Returns the footer message.
- (NSString*)footerMessage {
  CHECK([_userEmail length] > 0);
  return _moveToAccountFromSettings
             ? @""
             : l10n_util::GetNSStringF(
                   IDS_IOS_SETTINGS_AUTOFILL_ACCOUNT_ADDRESS_FOOTER_TEXT,
                   base::SysNSStringToUTF16(_userEmail));
}

// Returns the error message combined with footer.
- (NSAttributedString*)errorAndFooterMessage {
  CHECK([_delegate requiredFieldsWithEmptyValuesCount] > 0);
  CHECK(_settingsView);
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
      break;
  }
  return NO;
}

// Recomputes the required fields that are empty.
- (void)findRequiredFieldsWithEmptyValues {
  [_delegate resetRequiredFieldsWithEmptyValuesCount];
  for (TableViewItem* item in [_controller.tableViewModel
           itemsInSectionWithIdentifier:
               AutofillProfileDetailsSectionIdentifierFields]) {
    if (item.type == AutofillProfileDetailsItemTypeCountrySelectionField) {
      TableViewMultiDetailTextItem* multiDetailTextItem =
          base::apple::ObjCCastStrict<TableViewMultiDetailTextItem>(item);
      multiDetailTextItem.trailingDetailText = [self countryFieldCurrentValue];
    } else if ([self isItemTypeTextEditCell:item.type]) {
      // No requirement checks for local profiles.
      if (self.accountProfile || self.migrationPrompt ||
          _moveToAccountFromSettings) {
        TableViewTextEditItem* tableViewTextEditItem =
            base::apple::ObjCCastStrict<TableViewTextEditItem>(item);
        [self computeErrorIfRequiredTextField:tableViewTextEditItem];
      }
    }
  }

  [self reconfigureCells];

  if (_settingsView) {
    [self updateDoneButtonStatus];
  } else {
    [self updateSaveButtonStatus];
  }
}

// Returns the currently selected country value.
- (NSString*)countryFieldCurrentValue {
  return self.fieldValuesMap[[self countryFieldKeyValue]];
}

// Returns the key in the `self.fieldValuesMap` for the country field.
- (NSString*)countryFieldKeyValue {
  return base::SysUTF8ToNSString(
      autofill::FieldTypeToString(autofill::ADDRESS_HOME_COUNTRY));
}

@end
