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
#import "ios/chrome/browser/ui/autofill/autofill_ui_type.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type_util.h"
#import "ios/chrome/browser/ui/autofill/cells/autofill_edit_item.h"
#import "ios/chrome/browser/ui/autofill/cells/country_item.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
using ::AutofillTypeFromAutofillUIType;
using ::AutofillUITypeFromAutofillType;

// A constant to separate the error and the footer text.
const CGFloat kLineSpacingBetweenErrorAndFooter = 12.0f;

}  // namespace

@interface AutofillProfileEditTableViewController ()

// The AutofillProfileEditTableViewControllerDelegate for this ViewController.
@property(nonatomic, weak) id<AutofillProfileEditTableViewControllerDelegate>
    delegate;

// Stores the value displayed in the fields.
@property(nonatomic, strong) NSString* honorificPrefix;
@property(nonatomic, strong) NSString* companyName;
@property(nonatomic, strong) NSString* fullName;
@property(nonatomic, strong) NSString* homeAddressLine1;
@property(nonatomic, strong) NSString* homeAddressLine2;
@property(nonatomic, strong) NSString* homeAddressCity;
@property(nonatomic, strong) NSString* homeAddressState;
@property(nonatomic, strong) NSString* homeAddressZip;
@property(nonatomic, strong) NSString* homeAddressCountry;
@property(nonatomic, strong) NSString* homePhoneWholeNumber;
@property(nonatomic, strong) NSString* emailAddress;

// Stores the required field names whose values are empty;
@property(nonatomic, strong)
    NSMutableSet<NSString*>* requiredFieldsWithEmptyValue;

// Yes, if the error section has been presented.
@property(nonatomic, assign) BOOL errorSectionPresented;

// If YES, denote that the particular field requires a value.
@property(nonatomic, assign) BOOL nameRequired;
@property(nonatomic, assign) BOOL line1Required;
@property(nonatomic, assign) BOOL cityRequired;
@property(nonatomic, assign) BOOL stateRequired;
@property(nonatomic, assign) BOOL zipRequired;

// YES, if the profile's source is autofill::AutofillProfile::Source::kAccount.
@property(nonatomic, assign) BOOL accountProfile;

// The shown view controller.
@property(nonatomic, weak) ChromeTableViewController* controller;

// If YES, denotes that the view is shown in the settings.
@property(nonatomic, assign) BOOL settingsView;

// Points to the save/update button in the modal view.
@property(nonatomic, strong) TableViewTextButtonItem* modalSaveUpdateButton;

// If YES, denotes that the view is laid out for the migration prompt.
@property(nonatomic, assign) BOOL migrationPrompt;

// Denotes that the views are laid out to migrate an incomplete profile to
// account from the settings.
@property(nonatomic, assign) BOOL moveToAccountFromSettings;

@end

@implementation AutofillProfileEditTableViewController {
  NSString* _userEmail;
}

@synthesize moveToAccountFromSettings = _moveToAccountFromSettings;

#pragma mark - Initialization

- (instancetype)initWithDelegate:
                    (id<AutofillProfileEditTableViewControllerDelegate>)delegate
                       userEmail:(NSString*)userEmail
                      controller:(ChromeTableViewController*)controller
                    settingsView:(BOOL)settingsView {
  self = [super init];
  if (self) {
    _delegate = delegate;
    _userEmail = userEmail;
    _errorSectionPresented = NO;
    _accountProfile = NO;
    _requiredFieldsWithEmptyValue = [[NSMutableSet<NSString*> alloc] init];
    _controller = controller;
    _settingsView = settingsView;
    _moveToAccountFromSettings = NO;
  }

  return self;
}

#pragma mark - AutofillProfileEditHandler

- (void)viewDidDisappear {
  [self.delegate viewDidDisappear];
}

- (void)updateProfileData {
  TableViewModel* model = self.controller.tableViewModel;
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
    NSInteger itemType =
        [self.controller.tableViewModel itemTypeForIndexPath:path];

    if (itemType == AutofillProfileDetailsItemTypeCountry) {
        [self.delegate
            updateProfileMetadataWithValue:self.homeAddressCountry
                         forAutofillUIType:
                             AutofillUITypeProfileHomeAddressCountry];
        continue;
    } else if (![self isItemTypeTextEditCell:itemType]) {
      continue;
    }

    AutofillEditItem* item = base::apple::ObjCCastStrict<AutofillEditItem>(
        [model itemAtIndexPath:path]);
    [self.delegate updateProfileMetadataWithValue:item.textFieldValue
                                forAutofillUIType:item.autofillUIType];
  }
}

- (void)reconfigureCells {
  [self.controller reconfigureCellsForItems:
                       [self.controller.tableViewModel
                           itemsInSectionWithIdentifier:
                               AutofillProfileDetailsSectionIdentifierFields]];
}

- (void)loadModel {
  TableViewModel* model = self.controller.tableViewModel;

  [model
      addSectionWithIdentifier:AutofillProfileDetailsSectionIdentifierFields];
  for (size_t i = 0; i < std::size(kProfileFieldsToDisplay); ++i) {
    const AutofillProfileFieldDisplayInfo& field = kProfileFieldsToDisplay[i];

    if (field.autofillType == autofill::NAME_HONORIFIC_PREFIX &&
        !base::FeatureList::IsEnabled(
            autofill::features::kAutofillEnableSupportForHonorificPrefixes)) {
      continue;
    }

    if (AutofillUITypeFromAutofillType(field.autofillType) ==
        AutofillUITypeProfileHomeAddressCountry) {
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
      [self.controller.tableViewModel itemTypeForIndexPath:indexPath];
  if (itemType == AutofillProfileDetailsItemTypeFooter ||
      itemType == AutofillProfileDetailsItemTypeError) {
    if (!self.settingsView) {
      cell.separatorInset = UIEdgeInsetsMake(
          0, self.controller.tableView.bounds.size.width, 0, 0);
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

  if (itemType == AutofillProfileDetailsItemTypeCountry) {
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
      [self.controller.tableViewModel itemTypeForIndexPath:indexPath];
  if ([self showEditView]) {
    if (itemType == AutofillProfileDetailsItemTypeCountry) {
      [self.delegate willSelectCountryWithCurrentlySelectedCountry:
                         self.homeAddressCountry];
    } else if (itemType != AutofillProfileDetailsItemTypeFooter &&
               itemType != AutofillProfileDetailsItemTypeError) {
      UITableViewCell* cell =
          [self.controller.tableView cellForRowAtIndexPath:indexPath];
      TableViewTextEditCell* textFieldCell =
          base::apple::ObjCCastStrict<TableViewTextEditCell>(cell);
      [textFieldCell.textField becomeFirstResponder];
    }
  }
}

- (BOOL)heightForHeaderShouldBeZeroInSection:(NSInteger)section {
  NSInteger sectionIdentifier =
      [self.controller.tableViewModel sectionIdentifierForSectionIndex:section];

  return sectionIdentifier == AutofillProfileDetailsSectionIdentifierFooter ||
         sectionIdentifier ==
             AutofillProfileDetailsSectionIdentifierErrorFooter;
}

- (BOOL)heightForFooterShouldBeZeroInSection:(NSInteger)section {
  NSInteger sectionIdentifier =
      [self.controller.tableViewModel sectionIdentifierForSectionIndex:section];

  return (sectionIdentifier == AutofillProfileDetailsSectionIdentifierFields) ||
         (!self.settingsView &&
          sectionIdentifier == AutofillProfileDetailsSectionIdentifierFooter);
}

- (void)loadFooterForSettings {
  CHECK(self.settingsView);
  TableViewModel* model = self.controller.tableViewModel;

  if (self.accountProfile && _userEmail != nil) {
    [model
        addSectionWithIdentifier:AutofillProfileDetailsSectionIdentifierFooter];
    [model setFooter:[self footerItem]
        forSectionWithIdentifier:AutofillProfileDetailsSectionIdentifierFooter];
  }
}

- (void)loadMessageAndButtonForModalIfSaveOrUpdate:(BOOL)update {
  CHECK(!self.settingsView);
  TableViewModel* model = self.controller.tableViewModel;
  if (self.accountProfile || self.migrationPrompt) {
    DCHECK([_userEmail length] > 0);
    [model addItem:[self footerItemForModalViewIfSaveOrUpdate:update]
        toSectionWithIdentifier:AutofillProfileDetailsSectionIdentifierFields];
  }

  [model addItem:[self saveButtonIfSaveOrUpdate:update]
      toSectionWithIdentifier:AutofillProfileDetailsSectionIdentifierFields];
}

- (BOOL)isItemAtIndexPathTextEditCell:(NSIndexPath*)cellPath {
  NSInteger itemType =
      [self.controller.tableViewModel itemTypeForIndexPath:cellPath];
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
  [self.controller reconfigureCellsForItems:@[ tableViewItem ]];
}

- (void)tableViewItemDidChange:(TableViewTextEditItem*)tableViewItem {
  if ((self.accountProfile || self.migrationPrompt ||
       self.moveToAccountFromSettings)) {
    [self computeErrorIfRequiredTextField:tableViewItem];
    if (self.settingsView) {
      [self updateDoneButtonStatus];
    } else {
      [self updateSaveButtonStatus];
    }
  }
  [self.controller reconfigureCellsForItems:@[ tableViewItem ]];
}

- (void)tableViewItemDidEndEditing:(TableViewTextEditItem*)tableViewItem {
  [self.controller reconfigureCellsForItems:@[ tableViewItem ]];
}

#pragma mark - AutofillProfileEditConsumer

- (void)didSelectCountry:(NSString*)country {
  self.homeAddressCountry = country;
  [self findRequiredFieldsWithEmptyValues];
}

#pragma mark - Actions

- (void)didTapSaveButton {
  CHECK(!self.settingsView);
  [self updateProfileData];
  [self.delegate didSaveProfileFromModal];
}

#pragma mark - Conversion Helper Methods

// Returns `autofill::ServerFieldType` corresponding to the `itemType`.
- (autofill::ServerFieldType)serverFieldTypeCorrespondingToRequiredItemType:
    (AutofillProfileDetailsItemType)itemType {
  switch (itemType) {
    case AutofillProfileDetailsItemTypeFullName:
      return autofill::NAME_FULL;
    case AutofillProfileDetailsItemTypeLine1:
      return autofill::ADDRESS_HOME_LINE1;
    case AutofillProfileDetailsItemTypeCity:
      return autofill::ADDRESS_HOME_CITY;
    case AutofillProfileDetailsItemTypeState:
      return autofill::ADDRESS_HOME_STATE;
    case AutofillProfileDetailsItemTypeZip:
      return autofill::ADDRESS_HOME_ZIP;
    case AutofillProfileDetailsItemTypeHonorificPrefix:
    case AutofillProfileDetailsItemTypeCompanyName:
    case AutofillProfileDetailsItemTypeLine2:
    case AutofillProfileDetailsItemTypePhoneNumber:
    case AutofillProfileDetailsItemTypeEmailAddress:
    case AutofillProfileDetailsItemTypeCountry:
    case AutofillProfileDetailsItemTypeError:
    case AutofillProfileDetailsItemTypeFooter:
    case AutofillProfileDetailsItemTypeSaveButton:
    case AutofillProfileDetailsItemTypeMigrateToAccountButton:
    case AutofillProfileDetailsItemTypeMigrateToAccountRecommendation:
      break;
  }
  NOTREACHED();
  return autofill::UNKNOWN_TYPE;
}

// Returns the label corresponding to the item type for a required field.
- (NSString*)labelCorrespondingToRequiredItemType:
    (AutofillProfileDetailsItemType)itemType {
  switch (itemType) {
    case AutofillProfileDetailsItemTypeFullName:
      return l10n_util::GetNSString(IDS_IOS_AUTOFILL_FULLNAME);
    case AutofillProfileDetailsItemTypeLine1:
      return l10n_util::GetNSString(IDS_IOS_AUTOFILL_ADDRESS1);
    case AutofillProfileDetailsItemTypeCity:
      return l10n_util::GetNSString(IDS_IOS_AUTOFILL_CITY);
    case AutofillProfileDetailsItemTypeState:
      return l10n_util::GetNSString(IDS_IOS_AUTOFILL_STATE);
    case AutofillProfileDetailsItemTypeZip:
      return l10n_util::GetNSString(IDS_IOS_AUTOFILL_ZIP);
    case AutofillProfileDetailsItemTypeHonorificPrefix:
    case AutofillProfileDetailsItemTypeCompanyName:
    case AutofillProfileDetailsItemTypeLine2:
    case AutofillProfileDetailsItemTypePhoneNumber:
    case AutofillProfileDetailsItemTypeEmailAddress:
    case AutofillProfileDetailsItemTypeCountry:
    case AutofillProfileDetailsItemTypeError:
    case AutofillProfileDetailsItemTypeFooter:
    case AutofillProfileDetailsItemTypeSaveButton:
    case AutofillProfileDetailsItemTypeMigrateToAccountButton:
    case AutofillProfileDetailsItemTypeMigrateToAccountRecommendation:
      break;
  }
  NOTREACHED();
  return @"";
}

// Returns the value corresponding to `autofillType`.
- (NSString*)valueForAutofillUIType:(AutofillUIType)autofillUIType {
  switch (autofillUIType) {
    case AutofillUITypeProfileHonorificPrefix:
      return self.honorificPrefix;
    case AutofillUITypeProfileCompanyName:
      return self.companyName;
    case AutofillUITypeProfileFullName:
      return self.fullName;
    case AutofillUITypeProfileHomeAddressLine1:
      return self.homeAddressLine1;
    case AutofillUITypeProfileHomeAddressLine2:
      return self.homeAddressLine2;
    case AutofillUITypeProfileHomeAddressCity:
      return self.homeAddressCity;
    case AutofillUITypeProfileHomeAddressState:
      return self.homeAddressState;
    case AutofillUITypeProfileHomeAddressZip:
      return self.homeAddressZip;
    case AutofillUITypeProfileHomeAddressCountry:
      return self.homeAddressCountry;
    case AutofillUITypeProfileHomePhoneWholeNumber:
      return self.homePhoneWholeNumber;
    case AutofillUITypeProfileEmailAddress:
      return self.emailAddress;
    default:
      break;
  }
  NOTREACHED();
  return @"";
}

// Returns the item type corresponding to the `autofillUIType`.
- (AutofillProfileDetailsItemType)itemTypeForAutofillUIType:
    (AutofillUIType)autofillUIType {
  switch (autofillUIType) {
    case AutofillUITypeProfileHonorificPrefix:
      return AutofillProfileDetailsItemTypeHonorificPrefix;
    case AutofillUITypeProfileCompanyName:
      return AutofillProfileDetailsItemTypeCompanyName;
    case AutofillUITypeProfileFullName:
      return AutofillProfileDetailsItemTypeFullName;
    case AutofillUITypeProfileHomeAddressLine1:
      return AutofillProfileDetailsItemTypeLine1;
    case AutofillUITypeProfileHomeAddressLine2:
      return AutofillProfileDetailsItemTypeLine2;
    case AutofillUITypeProfileHomeAddressCity:
      return AutofillProfileDetailsItemTypeCity;
    case AutofillUITypeProfileHomeAddressState:
      return AutofillProfileDetailsItemTypeState;
    case AutofillUITypeProfileHomeAddressZip:
      return AutofillProfileDetailsItemTypeZip;
    case AutofillUITypeProfileHomeAddressCountry:
      return AutofillProfileDetailsItemTypeCountry;
    case AutofillUITypeProfileHomePhoneWholeNumber:
      return AutofillProfileDetailsItemTypePhoneNumber;
    case AutofillUITypeProfileEmailAddress:
      return AutofillProfileDetailsItemTypeEmailAddress;
    default:
      break;
  }
  NOTREACHED();
  return AutofillProfileDetailsItemTypeError;
}

#pragma mark - Items

// Creates and returns the `TableViewLinkHeaderFooterItem` footer item.
- (TableViewLinkHeaderFooterItem*)footerItem {
  CHECK(self.settingsView);
  TableViewLinkHeaderFooterItem* item = [[TableViewLinkHeaderFooterItem alloc]
      initWithType:AutofillProfileDetailsItemTypeFooter];
  item.text = [self footerMessage];
  return item;
}

// Returns the error message item as a footer.
- (TableViewAttributedStringHeaderFooterItem*)errorMessageItem {
  CHECK(self.settingsView);
  TableViewAttributedStringHeaderFooterItem* item =
      [[TableViewAttributedStringHeaderFooterItem alloc]
          initWithType:AutofillProfileDetailsItemTypeError];
  [item setAttributedString:[self errorAndFooterMessage]];
  return item;
}

// Returns text fields displaying autofill field info used in save/update
// prompts as well as the settings view.
- (AutofillEditItem*)autofillEditItemFromField:
    (const AutofillProfileFieldDisplayInfo&)field {
  AutofillUIType autofillUIType =
      AutofillUITypeFromAutofillType(field.autofillType);
  AutofillEditItem* item = [[AutofillEditItem alloc]
      initWithType:[self itemTypeForAutofillUIType:autofillUIType]];
  item.fieldNameLabelText = l10n_util::GetNSString(field.displayStringID);
  item.textFieldValue = [self valueForAutofillUIType:autofillUIType];
  item.autofillUIType = autofillUIType;
  item.textFieldEnabled = [self showEditView];
  item.hideIcon = ![self showEditView];
  item.autoCapitalizationType = field.autoCapitalizationType;
  item.returnKeyType =
      self.settingsView ? field.returnKeyType : UIReturnKeyDone;
  item.keyboardType = field.keyboardType;
  item.delegate = self;
  item.accessibilityIdentifier = l10n_util::GetNSString(field.displayStringID);
  item.useCustomSeparator = !self.settingsView;
  return item;
}

// Returns the country field used in the save/update prompts as well as the
// settings view.
- (TableViewMultiDetailTextItem*)countryItem {
  TableViewMultiDetailTextItem* item = [[TableViewMultiDetailTextItem alloc]
      initWithType:AutofillProfileDetailsItemTypeCountry];
  item.text = l10n_util::GetNSString(IDS_IOS_AUTOFILL_COUNTRY);
  item.trailingDetailText = self.homeAddressCountry;
  item.trailingDetailTextColor = [UIColor colorNamed:kTextPrimaryColor];
  item.useCustomSeparator = !self.settingsView;
  if (!self.settingsView) {
    item.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  } else if (self.controller.tableView.editing) {
    item.editingAccessoryType = UITableViewCellAccessoryDisclosureIndicator;
  }
  return item;
}

// Returns the footer element for the save/update prompts.
- (TableViewTextItem*)footerItemForModalViewIfSaveOrUpdate:(BOOL)update {
  CHECK(!self.settingsView);
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
  CHECK(!self.settingsView);
  self.modalSaveUpdateButton = [[TableViewTextButtonItem alloc]
      initWithType:AutofillProfileDetailsItemTypeSaveButton];
  self.modalSaveUpdateButton.textAlignment = NSTextAlignmentNatural;
  if (self.migrationPrompt) {
    self.modalSaveUpdateButton.buttonText = l10n_util::GetNSString(
        IDS_AUTOFILL_ADDRESS_MIGRATION_TO_ACCOUNT_PROMPT_OK_BUTTON_LABEL);
  } else {
    self.modalSaveUpdateButton.buttonText = l10n_util::GetNSString(
        update ? IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_OK_BUTTON_LABEL
               : IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_OK_BUTTON_LABEL);
  }

  self.modalSaveUpdateButton.disableButtonIntrinsicWidth = YES;

  return self.modalSaveUpdateButton;
}

#pragma mark - Private

// Returns true if the itemType belongs to a required field.
- (BOOL)isItemTypeRequiredField:(AutofillProfileDetailsItemType)itemType {
  switch (itemType) {
    case AutofillProfileDetailsItemTypeFullName:
      return self.nameRequired;
    case AutofillProfileDetailsItemTypeLine1:
      return self.line1Required;
    case AutofillProfileDetailsItemTypeCity:
      return self.cityRequired;
    case AutofillProfileDetailsItemTypeState:
      return self.stateRequired;
    case AutofillProfileDetailsItemTypeZip:
      return self.zipRequired;
    case AutofillProfileDetailsItemTypeHonorificPrefix:
    case AutofillProfileDetailsItemTypeCompanyName:
    case AutofillProfileDetailsItemTypeLine2:
    case AutofillProfileDetailsItemTypePhoneNumber:
    case AutofillProfileDetailsItemTypeEmailAddress:
    case AutofillProfileDetailsItemTypeCountry:
    case AutofillProfileDetailsItemTypeError:
    case AutofillProfileDetailsItemTypeFooter:
    case AutofillProfileDetailsItemTypeSaveButton:
    case AutofillProfileDetailsItemTypeMigrateToAccountButton:
    case AutofillProfileDetailsItemTypeMigrateToAccountRecommendation:
      break;
  }
  return NO;
}

// Computes whether the `tableViewItem` is a required field and empty.
- (void)computeErrorIfRequiredTextField:(TableViewTextEditItem*)tableViewItem {
  AutofillProfileDetailsItemType itemType =
      static_cast<AutofillProfileDetailsItemType>(tableViewItem.type);
  if (![self isItemTypeRequiredField:itemType] ||
      [self requiredFieldWasEmptyOnProfileLoadForItemType:itemType]) {
    // Early return if the text field is not a required field or contained an
    // empty value when the profile was loaded.
    tableViewItem.hasValidText = YES;
    return;
  }

  NSString* requiredTextFieldLabel =
      [self labelCorrespondingToRequiredItemType:itemType];
  BOOL isValueEmpty = (tableViewItem.textFieldValue.length == 0);

  // If the required text field contains a value now, remove it from
  // `self.requiredFieldsWithEmptyValue`.
  if ([self.requiredFieldsWithEmptyValue
          containsObject:requiredTextFieldLabel] &&
      !isValueEmpty) {
    [self.requiredFieldsWithEmptyValue removeObject:requiredTextFieldLabel];
    tableViewItem.hasValidText = YES;
  }

  // If the required field is empty, add it to
  // `self.requiredFieldsWithEmptyValue`.
  if (isValueEmpty) {
    [self.requiredFieldsWithEmptyValue addObject:requiredTextFieldLabel];
    tableViewItem.hasValidText = NO;
  }
}

// Returns YES if the profile contained an empty value for the required
// `itemType`.
- (BOOL)requiredFieldWasEmptyOnProfileLoadForItemType:
    (AutofillProfileDetailsItemType)itemType {
  DCHECK([self isItemTypeRequiredField:itemType]);

  if (self.moveToAccountFromSettings) {
    return NO;
  }

  return [self.delegate
      fieldValueEmptyOnProfileLoadForType:
          [self serverFieldTypeCorrespondingToRequiredItemType:itemType]];
}

// Removes the given section if it exists.
- (void)removeSectionWithIdentifier:(NSInteger)sectionIdentifier
                   withRowAnimation:(UITableViewRowAnimation)animation {
  CHECK(self.settingsView);
  TableViewModel* model = self.controller.tableViewModel;
  if ([model hasSectionForSectionIdentifier:sectionIdentifier]) {
    NSInteger section = [model sectionForSectionIdentifier:sectionIdentifier];
    [model removeSectionWithIdentifier:sectionIdentifier];
    [[self.controller tableView]
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
  CHECK(self.settingsView);
  TableViewModel* model = self.controller.tableViewModel;
  [self.controller
      performBatchTableViewUpdates:^{
        [self removeSectionWithIdentifier:removeSection
                         withRowAnimation:UITableViewRowAnimationTop];
        NSUInteger fieldsSectionIndex =
            [model sectionForSectionIdentifier:
                       AutofillProfileDetailsSectionIdentifierFields];
        [model insertSectionWithIdentifier:addSection
                                   atIndex:fieldsSectionIndex + 1];
        [self.controller.tableView
              insertSections:[NSIndexSet
                                 indexSetWithIndex:fieldsSectionIndex + 1]
            withRowAnimation:UITableViewRowAnimationTop];
        [self.controller.tableViewModel
                           setFooter:(([self.requiredFieldsWithEmptyValue
                                               count] > 0)
                                          ? [self errorMessageItem]
                                          : [self footerItem])
            forSectionWithIdentifier:addSection];
      }
                        completion:nil];
}

// Updates the Done button status based on `self.requiredFieldsWithEmptyValue`
// and shows/removes the error footer if required.
- (void)updateDoneButtonStatus {
  CHECK(self.settingsView);
  BOOL shouldShowError = ([self.requiredFieldsWithEmptyValue count] > 0);
  self.controller.navigationItem.rightBarButtonItem.enabled = !shouldShowError;
  if (shouldShowError != self.errorSectionPresented) {
    AutofillProfileDetailsSectionIdentifier addSection =
        shouldShowError ? AutofillProfileDetailsSectionIdentifierErrorFooter
                        : AutofillProfileDetailsSectionIdentifierFooter;
    AutofillProfileDetailsSectionIdentifier removeSection =
        shouldShowError ? AutofillProfileDetailsSectionIdentifierFooter
                        : AutofillProfileDetailsSectionIdentifierErrorFooter;
    [self changeFooterStatusToRemoveSection:removeSection
                                 addSection:addSection];
    self.errorSectionPresented = shouldShowError;
  } else if (shouldShowError && [self shouldChangeErrorMessage]) {
    [self
        changeFooterStatusToRemoveSection:
            AutofillProfileDetailsSectionIdentifierErrorFooter
                               addSection:
                                   AutofillProfileDetailsSectionIdentifierErrorFooter];
  }
}

// Updates the Save/Update button status based on
// `self.requiredFieldsWithEmptyValue`.
- (void)updateSaveButtonStatus {
  CHECK(!self.settingsView);
  BOOL shouldShowError = ([self.requiredFieldsWithEmptyValue count] > 0);
  self.modalSaveUpdateButton.enabled = !shouldShowError;
  [self.controller reconfigureCellsForItems:@[ self.modalSaveUpdateButton ]];
}

// Returns YES, if the error message needs to be changed. This happens when
// there are multiple required fields that become empty.
- (BOOL)shouldChangeErrorMessage {
  CHECK(self.settingsView);
  TableViewHeaderFooterItem* currentFooter = [self.controller.tableViewModel
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
  return !self.settingsView || self.controller.tableView.editing;
}

// Returns the footer message.
- (NSString*)footerMessage {
  CHECK([_userEmail length] > 0);
  return self.moveToAccountFromSettings
             ? @""
             : l10n_util::GetNSStringF(
                   IDS_IOS_SETTINGS_AUTOFILL_ACCOUNT_ADDRESS_FOOTER_TEXT,
                   base::SysNSStringToUTF16(_userEmail));
}

// Returns the error message combined with footer.
- (NSAttributedString*)errorAndFooterMessage {
  CHECK([self.requiredFieldsWithEmptyValue count] > 0);
  CHECK(self.settingsView);
  NSString* error = l10n_util::GetPluralNSStringF(
      IDS_IOS_SETTINGS_EDIT_AUTOFILL_ADDRESS_REQUIREMENT_ERROR,
      (int)[self.requiredFieldsWithEmptyValue count]);

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
    case AutofillProfileDetailsItemTypeHonorificPrefix:
    case AutofillProfileDetailsItemTypeCompanyName:
    case AutofillProfileDetailsItemTypeFullName:
    case AutofillProfileDetailsItemTypeLine1:
    case AutofillProfileDetailsItemTypeLine2:
    case AutofillProfileDetailsItemTypeCity:
    case AutofillProfileDetailsItemTypeState:
    case AutofillProfileDetailsItemTypeZip:
    case AutofillProfileDetailsItemTypePhoneNumber:
    case AutofillProfileDetailsItemTypeEmailAddress:
      return YES;
    case AutofillProfileDetailsItemTypeCountry:
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
  [self.requiredFieldsWithEmptyValue removeAllObjects];
  for (TableViewItem* item in [self.controller.tableViewModel
           itemsInSectionWithIdentifier:
               AutofillProfileDetailsSectionIdentifierFields]) {
    if (item.type == AutofillProfileDetailsItemTypeCountry) {
      TableViewMultiDetailTextItem* multiDetailTextItem =
          base::apple::ObjCCastStrict<TableViewMultiDetailTextItem>(item);
      multiDetailTextItem.trailingDetailText = self.homeAddressCountry;
    } else if ([self isItemTypeTextEditCell:item.type]) {
      // No requirement checks for local profiles.
      if (self.accountProfile || self.migrationPrompt ||
          self.moveToAccountFromSettings) {
        TableViewTextEditItem* tableViewTextEditItem =
            base::apple::ObjCCastStrict<TableViewTextEditItem>(item);
        [self computeErrorIfRequiredTextField:tableViewTextEditItem];
      }
    }
  }

  [self reconfigureCells];

  if (self.settingsView) {
    [self updateDoneButtonStatus];
  } else {
    [self updateSaveButtonStatus];
  }
}

@end
