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
#import "ios/chrome/browser/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
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
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeField = kItemTypeEnumZero,
  ItemTypeCountry,
  ItemTypeFooter
};

}  // namespace

@interface AutofillProfileEditTableViewController ()

// The AutofillProfileEditTableViewControllerDelegate for this ViewController.
@property(nonatomic, weak) id<AutofillProfileEditTableViewControllerDelegate>
    delegate;

// Stores the value displayed in the country field.
@property(nonatomic, strong) NSString* countryValue;

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

#pragma mark - Public

- (void)didSelectCountry:(NSString*)selectedCountry {
  self.countryValue = selectedCountry;
  // Reload the model.
  [self loadModel];
  // Update the cells.
  [self reconfigureCellsForItems:
            [self.tableViewModel
                itemsInSectionWithIdentifier:SectionIdentifierFields]];
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

    [self.delegate autofillProfileEditViewController:self
                              didEditAutofillProfile:_autofillProfile];
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
    [model setFooter:[self footerItem]
        forSectionWithIdentifier:SectionIdentifierFields];
  }
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
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
    if (itemType == ItemTypeField) {
      UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:indexPath];
      TableViewTextEditCell* textFieldCell =
          base::mac::ObjCCastStrict<TableViewTextEditCell>(cell);
      [textFieldCell.textField becomeFirstResponder];
    } else if (itemType == ItemTypeCountry) {
      [self.delegate autofillProfileEditViewController:self
          willSelectCountryWithCurrentlySelectedCountry:self.countryValue];
    }
  }
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

#pragma mark - Private

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
  AutofillEditItem* item =
      [[AutofillEditItem alloc] initWithType:ItemTypeField];
  item.fieldNameLabelText = l10n_util::GetNSString(field.displayStringID);
  item.textFieldValue = base::SysUTF16ToNSString(_autofillProfile->GetInfo(
      autofill::AutofillType(field.autofillType),
      GetApplicationContext()->GetApplicationLocale()));
  item.autofillUIType = AutofillUITypeFromAutofillType(field.autofillType);
  item.textFieldEnabled = self.tableView.editing;
  item.hideIcon = !self.tableView.editing;
  item.autoCapitalizationType = field.autoCapitalizationType;
  item.returnKeyType = field.returnKeyType;
  item.keyboardType = field.keyboardType;
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

@end
