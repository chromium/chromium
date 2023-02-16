// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill/autofill_profile_edit_table_view_controller.h"

#import "base/mac/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_model/autofill_profile.h"
#import "components/autofill/core/browser/field_types.h"
#import "components/autofill/core/common/autofill_features.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type_util.h"
#import "ios/chrome/browser/ui/autofill/cells/autofill_edit_item.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_constants.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_profile_edit_table_view_controller_delegate.h"
#import "ios/chrome/browser/ui/settings/autofill/cells/country_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_multi_detail_text_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_edit_item_delegate.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif


namespace {
using ::AutofillTypeFromAutofillUIType;
using ::AutofillUITypeFromAutofillType;

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierFields = kSectionIdentifierEnumZero,
  SectionIdentifierError,
  SectionIdentifierFooter
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeField = kItemTypeEnumZero,
  ItemTypeCountry,
  ItemTypeLine1,
  ItemTypeCity,
  ItemTypeState,
  ItemTypeZip,
  ItemTypeError,
  ItemTypeFooter
};

}  // namespace

@interface AutofillProfileEditTableViewController () <
    TableViewTextEditItemDelegate>

// The AutofillProfileEditTableViewControllerDelegate for this ViewController.
@property(nonatomic, weak) id<AutofillProfileEditTableViewControllerDelegate>
    delegate;

// Stores the value displayed in the country field.
@property(nonatomic, strong) NSString* countryValue;

// Stores the required field names whose values are empty;
@property(nonatomic, strong)
    NSMutableSet<NSString*>* requiredFieldsWithEmptyValue;

// Yes, if the error section has been presented.
@property(nonatomic, assign) BOOL errorSectionPresented;

// If YES, denote that the particular field requires a value.
@property(nonatomic, assign) BOOL line1Required;
@property(nonatomic, assign) BOOL cityRequired;
@property(nonatomic, assign) BOOL stateRequired;
@property(nonatomic, assign) BOOL zipRequired;

@end

@implementation AutofillProfileEditTableViewController {
  autofill::AutofillProfile* _autofillProfile;
  NSString* _userEmail;
}

#pragma mark - Initialization

- (instancetype)initWithDelegate:
                    (id<AutofillProfileEditTableViewControllerDelegate>)delegate
                         profile:(autofill::AutofillProfile*)profile
                       userEmail:(NSString*)userEmail {
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _delegate = delegate;
    _autofillProfile = profile;
    _userEmail = userEmail;
    _errorSectionPresented = NO;
    _requiredFieldsWithEmptyValue = [[NSMutableSet<NSString*> alloc] init];

    [self setTitle:l10n_util::GetNSString(IDS_IOS_AUTOFILL_EDIT_ADDRESS)];
  }

  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.tableView.allowsSelectionDuringEditing = YES;
  self.tableView.accessibilityIdentifier = kAutofillProfileEditTableViewId;
  self.countryValue = base::SysUTF16ToNSString(_autofillProfile->GetInfo(
      autofill::ServerFieldType::ADDRESS_HOME_COUNTRY,
      GetApplicationContext()->GetApplicationLocale()));

  [self loadModel];
}

- (void)viewDidDisappear:(BOOL)animated {
  [self.delegate viewDidDisappear];
  [super viewDidDisappear:animated];
}

#pragma mark - SettingsRootTableViewController

- (void)editButtonPressed {
  [super editButtonPressed];

  if (!self.tableView.editing) {
    TableViewModel* model = self.tableViewModel;
    NSInteger itemCount =
        [model numberOfItemsInSection:
                   [model sectionForSectionIdentifier:SectionIdentifierFields]];

    // Reads the values from the fields and updates the local copy of the
    // profile accordingly.
    NSInteger section =
        [model sectionForSectionIdentifier:SectionIdentifierFields];
    for (NSInteger itemIndex = 0; itemIndex < itemCount; ++itemIndex) {
      NSIndexPath* path = [NSIndexPath indexPathForItem:itemIndex
                                              inSection:section];
      NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:path];
      if (itemType == ItemTypeFooter || itemType == ItemTypeError) {
        continue;
      }
      if (itemType == ItemTypeCountry) {
        _autofillProfile->SetInfoWithVerificationStatus(
            autofill::AutofillType(
                autofill::ServerFieldType::ADDRESS_HOME_COUNTRY),
            base::SysNSStringToUTF16(self.countryValue),
            GetApplicationContext()->GetApplicationLocale(),
            autofill::VerificationStatus::kUserVerified);
        continue;
      }
      AutofillEditItem* item = base::mac::ObjCCastStrict<AutofillEditItem>(
          [model itemAtIndexPath:path]);
      autofill::ServerFieldType serverFieldType =
          AutofillTypeFromAutofillUIType(item.autofillUIType);

      // Since the country field is a text field, we should use SetInfo() to
      // make sure they get converted to country codes.
      // Use SetInfo for fullname to propogate the change to the name_first,
      // name_middle and name_last subcomponents.
      if (item.autofillUIType == AutofillUITypeProfileHomeAddressCountry ||
          item.autofillUIType == AutofillUITypeProfileFullName) {
        _autofillProfile->SetInfoWithVerificationStatus(
            autofill::AutofillType(serverFieldType),
            base::SysNSStringToUTF16(item.textFieldValue),
            GetApplicationContext()->GetApplicationLocale(),
            autofill::VerificationStatus::kUserVerified);
      } else {
        _autofillProfile->SetRawInfoWithVerificationStatus(
            serverFieldType, base::SysNSStringToUTF16(item.textFieldValue),
            autofill::VerificationStatus::kUserVerified);
      }
    }

    [self.delegate didEditAutofillProfile:_autofillProfile];
  }

  // Reload the model.
  [self loadModel];
  // Update the cells.
  [self reconfigureCellsForItems:
            [self.tableViewModel
                itemsInSectionWithIdentifier:SectionIdentifierFields]];
}

- (void)loadModel {
  [super loadModel];
  TableViewModel* model = self.tableViewModel;

  [model addSectionWithIdentifier:SectionIdentifierFields];
  for (size_t i = 0; i < std::size(kProfileFieldsToDisplay); ++i) {
    const AutofillProfileFieldDisplayInfo& field = kProfileFieldsToDisplay[i];

    if (field.autofillType == autofill::NAME_HONORIFIC_PREFIX &&
        !base::FeatureList::IsEnabled(
            autofill::features::kAutofillEnableSupportForHonorificPrefixes)) {
      continue;
    }

    if (base::FeatureList::IsEnabled(
            autofill::features::kAutofillAccountProfilesUnionView) &&
        AutofillUITypeFromAutofillType(field.autofillType) ==
            AutofillUITypeProfileHomeAddressCountry) {
      [model addItem:[self countryItem]
          toSectionWithIdentifier:SectionIdentifierFields];
    } else {
      [model addItem:[self autofillEditItemFromField:field]
          toSectionWithIdentifier:SectionIdentifierFields];
    }
  }

  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillAccountProfilesUnionView) &&
      _autofillProfile->source() ==
          autofill::AutofillProfile::Source::kAccount &&
      _userEmail != nil) {
    [model addSectionWithIdentifier:SectionIdentifierFooter];
    // TODO(crbug.com/1407666): Fix the extra spacing between the footers.
    [model setFooter:[self footerItem]
        forSectionWithIdentifier:SectionIdentifierFooter];
  }
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  if (itemType == ItemTypeFooter || itemType == ItemTypeError) {
    return cell;
  }
  if (itemType == ItemTypeCountry) {
    TableViewMultiDetailTextCell* multiDetailTextCell =
        base::mac::ObjCCastStrict<TableViewMultiDetailTextCell>(cell);
    multiDetailTextCell.accessibilityIdentifier =
        multiDetailTextCell.textLabel.text;
    return multiDetailTextCell;
  }

  TableViewTextEditCell* textFieldCell =
      base::mac::ObjCCastStrict<TableViewTextEditCell>(cell);
  textFieldCell.accessibilityIdentifier = textFieldCell.textLabel.text;
  textFieldCell.textField.delegate = self;
  return textFieldCell;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  if (self.tableView.editing) {
    if (itemType == ItemTypeCountry) {
      [self.delegate
          willSelectCountryWithCurrentlySelectedCountry:self.countryValue];
    } else if (itemType != ItemTypeFooter && itemType != ItemTypeError) {
      UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:indexPath];
      TableViewTextEditCell* textFieldCell =
          base::mac::ObjCCastStrict<TableViewTextEditCell>(cell);
      [textFieldCell.textField becomeFirstResponder];
    }
  }
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  NSInteger sectionIdentifier =
      [self.tableViewModel sectionIdentifierForSectionIndex:section];

  if (sectionIdentifier == SectionIdentifierFooter ||
      sectionIdentifier == SectionIdentifierError) {
    return 0;
  }
  return [super tableView:tableView heightForHeaderInSection:section];
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  NSInteger sectionIdentifier =
      [self.tableViewModel sectionIdentifierForSectionIndex:section];
  if (sectionIdentifier == SectionIdentifierFields) {
    return 0;
  }
  return [super tableView:tableView heightForFooterInSection:section];
}

#pragma mark - TableViewTextEditItemDelegate

- (void)tableViewItemDidBeginEditing:(TableViewTextEditItem*)tableViewItem {
  [self reconfigureCellsForItems:@[ tableViewItem ]];
}

- (void)tableViewItemDidChange:(TableViewTextEditItem*)tableViewItem {
  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillAccountProfilesUnionView) &&
      _autofillProfile->source() ==
          autofill::AutofillProfile::Source::kAccount &&
      [self hasTableViewErrorStateChanged:tableViewItem]) {
    self.navigationItem.rightBarButtonItem.enabled =
        !([self.requiredFieldsWithEmptyValue count]);
  }
  [self reconfigureCellsForItems:@[ tableViewItem ]];
}

- (void)tableViewItemDidEndEditing:(TableViewTextEditItem*)tableViewItem {
  [self reconfigureCellsForItems:@[ tableViewItem ]];
}

#pragma mark - UITableViewDelegate

- (BOOL)tableView:(UITableView*)tableView
    canEditRowAtIndexPath:(NSIndexPath*)indexPath {
  // If we don't allow the edit of the cell, the selection of the cell isn't
  // forwarded.
  return YES;
}

- (UITableViewCellEditingStyle)tableView:(UITableView*)tableView
           editingStyleForRowAtIndexPath:(NSIndexPath*)indexPath {
  return UITableViewCellEditingStyleNone;
}

- (BOOL)tableView:(UITableView*)tableview
    shouldIndentWhileEditingRowAtIndexPath:(NSIndexPath*)indexPath {
  return NO;
}

#pragma mark - AutofillProfileEditConsumer

- (void)didSelectCountry:(NSString*)country {
  self.countryValue = country;

  // TODO(crbug.com/1407666): Should not reload the model, the user changed
  // fields may be overwritten.
  [self loadModel];
  // Update the cells.
  [self reconfigureCellsForItems:
            [self.tableViewModel
                itemsInSectionWithIdentifier:SectionIdentifierFields]];
}

#pragma mark - Private

// Returns true if the itemType belongs to a required field.
- (BOOL)isItemTypeRequiredField:(ItemType)itemType {
  return (itemType == ItemTypeLine1 && self.line1Required) ||
         (itemType == ItemTypeCity && self.cityRequired) ||
         (itemType == ItemTypeState && self.stateRequired) ||
         (itemType == ItemTypeZip && self.zipRequired);
}

// Returns YES if the error state has changed.
- (BOOL)hasTableViewErrorStateChanged:(TableViewTextEditItem*)tableViewItem {
  ItemType itemType = static_cast<ItemType>(tableViewItem.type);
  if (![self isItemTypeRequiredField:itemType]) {
    // Early return if the text field is not a required field.
    tableViewItem.hasValidText = YES;
    return NO;
  }

  NSString* requiredTextFieldLabel =
      [self labelCorrespondingToRequiredItemType:itemType];
  BOOL isValueEmpty = (tableViewItem.textFieldValue.length == 0);

  // If the required text field contains a value now, remove the error section.
  if ([self.requiredFieldsWithEmptyValue
          containsObject:requiredTextFieldLabel] &&
      !isValueEmpty) {
    [self.requiredFieldsWithEmptyValue removeObject:requiredTextFieldLabel];
    tableViewItem.hasValidText = YES;
    return [self removeErrorSection];
  }

  // If the required field is empty, show the error section.
  if (isValueEmpty) {
    [self.requiredFieldsWithEmptyValue addObject:requiredTextFieldLabel];
    tableViewItem.hasValidText = NO;
    return [self showErrorSection];
  }

  return NO;
}

// Returns the label corresponding to the item type for a required field.
- (NSString*)labelCorrespondingToRequiredItemType:(ItemType)itemType {
  DCHECK([self isItemTypeRequiredField:itemType]);
  if (itemType == ItemTypeLine1) {
    return l10n_util::GetNSString(IDS_IOS_AUTOFILL_ADDRESS1);
  }
  if (itemType == ItemTypeCity) {
    return l10n_util::GetNSString(IDS_IOS_AUTOFILL_CITY);
  }
  if (itemType == ItemTypeState) {
    return l10n_util::GetNSString(IDS_IOS_AUTOFILL_STATE);
  }
  return l10n_util::GetNSString(IDS_IOS_AUTOFILL_ZIP);
}

// Returns the error message item as a footer.
- (TableViewLinkHeaderFooterItem*)errorMessageItem {
  TableViewLinkHeaderFooterItem* item =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeFooter];
  // TODO(crbug.com/1407666): Add string compatible with i18n.
  if ([self.requiredFieldsWithEmptyValue count] > 1) {
    item.text =
        @"Test Some required fields are empty. Fill them before saving.";
  } else {
    item.text = @"Test A required field is empty. Fill it before saving.";
  }
  // TODO(crbug.com/1407666): Change color to kRedColor.
  return item;
}

// Creates and returns the `TableViewLinkHeaderFooterItem` footer item.
- (TableViewLinkHeaderFooterItem*)footerItem {
  TableViewLinkHeaderFooterItem* item =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeFooter];
  // TODO(crbug.com/1407666): Add footer string compatible with i18n.
  item.text = [NSString
      stringWithFormat:
          @"Test The address is saved in your Google Account(%@). "
          @"You can use the address across Google products on any device",
          _userEmail];
  return item;
}

- (AutofillEditItem*)autofillEditItemFromField:
    (const AutofillProfileFieldDisplayInfo&)field {
  ItemType itemType = ItemTypeField;
  AutofillUIType autofillUIType =
      AutofillUITypeFromAutofillType(field.autofillType);
  if (autofillUIType == AutofillUITypeProfileHomeAddressLine1) {
    itemType = ItemTypeLine1;
  } else if (autofillUIType == AutofillUITypeProfileHomeAddressCity) {
    itemType = ItemTypeCity;
  } else if (autofillUIType == AutofillUITypeProfileHomeAddressState) {
    itemType = ItemTypeState;
  } else if (autofillUIType == AutofillUITypeProfileHomeAddressZip) {
    itemType = ItemTypeZip;
  }
  AutofillEditItem* item = [[AutofillEditItem alloc] initWithType:itemType];
  item.fieldNameLabelText = l10n_util::GetNSString(field.displayStringID);
  item.textFieldValue = base::SysUTF16ToNSString(_autofillProfile->GetInfo(
      autofill::AutofillType(field.autofillType),
      GetApplicationContext()->GetApplicationLocale()));
  item.autofillUIType = autofillUIType;
  item.textFieldEnabled = self.tableView.editing;
  item.hideIcon = !self.tableView.editing;
  item.autoCapitalizationType = field.autoCapitalizationType;
  item.returnKeyType = field.returnKeyType;
  item.keyboardType = field.keyboardType;
  item.delegate = self;
  item.accessibilityIdentifier = l10n_util::GetNSString(field.displayStringID);
  return item;
}

- (TableViewMultiDetailTextItem*)countryItem {
  TableViewMultiDetailTextItem* item =
      [[TableViewMultiDetailTextItem alloc] initWithType:ItemTypeCountry];
  item.text = l10n_util::GetNSString(IDS_IOS_AUTOFILL_COUNTRY);
  item.trailingDetailText = self.countryValue;
  // TODO(crbug.com/1407666): Show it as a button in the edit mode and fix the
  // color.
  return item;
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

// Displays the error section if it is not shown. Returns YES, if it was not
// presented.
- (BOOL)showErrorSection {
  if (self.errorSectionPresented) {
    return NO;
  }

  TableViewModel* model = self.tableViewModel;
  [self
      performBatchTableViewUpdates:^{
        NSUInteger fieldsSectionIndex =
            [model sectionForSectionIdentifier:SectionIdentifierFields];
        [model insertSectionWithIdentifier:SectionIdentifierError
                                   atIndex:fieldsSectionIndex + 1];
        [self.tableView
              insertSections:[NSIndexSet
                                 indexSetWithIndex:fieldsSectionIndex + 1]
            withRowAnimation:UITableViewRowAnimationTop];
        [model setFooter:[self errorMessageItem]
            forSectionWithIdentifier:SectionIdentifierError];
      }
                        completion:nil];

  self.errorSectionPresented = YES;
  return YES;
}

// Removes the error section if it is shown and there are no required empty
// fields.
- (BOOL)removeErrorSection {
  if (!self.errorSectionPresented ||
      [self.requiredFieldsWithEmptyValue count] > 0) {
    return NO;
  }
  [self
      performBatchTableViewUpdates:^{
        [self removeSectionWithIdentifier:SectionIdentifierError
                         withRowAnimation:UITableViewRowAnimationTop];
      }
                        completion:nil];
  self.errorSectionPresented = NO;
  return YES;
}

@end
