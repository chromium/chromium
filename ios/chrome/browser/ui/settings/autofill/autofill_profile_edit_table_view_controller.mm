// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill/autofill_profile_edit_table_view_controller.h"

#import "base/mac/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_model/autofill_profile.h"
#import "components/autofill/core/browser/field_types.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/autofill/core/common/autofill_features.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type_util.h"
#import "ios/chrome/browser/ui/autofill/cells/autofill_edit_item.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_constants.h"
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
};

}  // namespace

@interface AutofillProfileEditTableViewController ()

// Initializes a AutofillProfileEditTableViewController with `profile` and
// `dataManager`.
- (instancetype)initWithProfile:(const autofill::AutofillProfile&)profile
            personalDataManager:(autofill::PersonalDataManager*)dataManager
    NS_DESIGNATED_INITIALIZER;

@end

@implementation AutofillProfileEditTableViewController {
  autofill::PersonalDataManager* _personalDataManager;  // weak
  autofill::AutofillProfile _autofillProfile;
}

#pragma mark - Initialization

- (instancetype)initWithProfile:(const autofill::AutofillProfile&)profile
            personalDataManager:(autofill::PersonalDataManager*)dataManager {
  DCHECK(dataManager);

  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _personalDataManager = dataManager;
    _autofillProfile = profile;

    [self setTitle:l10n_util::GetNSString(IDS_IOS_AUTOFILL_EDIT_ADDRESS)];
  }

  return self;
}

+ (instancetype)controllerWithProfile:(const autofill::AutofillProfile&)profile
                  personalDataManager:
                      (autofill::PersonalDataManager*)dataManager {
  return [[self alloc] initWithProfile:profile personalDataManager:dataManager];
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.tableView.allowsSelectionDuringEditing = YES;
  self.tableView.accessibilityIdentifier = kAutofillProfileEditTableViewId;
  [self loadModel];
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
        _autofillProfile.SetInfoWithVerificationStatus(
            autofill::AutofillType(serverFieldType),
            base::SysNSStringToUTF16(item.textFieldValue),
            GetApplicationContext()->GetApplicationLocale(),
            autofill::VerificationStatus::kUserVerified);
      } else {
        _autofillProfile.SetRawInfoWithVerificationStatus(
            serverFieldType, base::SysNSStringToUTF16(item.textFieldValue),
            autofill::VerificationStatus::kUserVerified);
      }
    }

    _personalDataManager->UpdateProfile(_autofillProfile);
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

  std::string locale = GetApplicationContext()->GetApplicationLocale();
  [model addSectionWithIdentifier:SectionIdentifierFields];
  for (size_t i = 0; i < std::size(kProfileFieldsToDisplay); ++i) {
    const AutofillProfileFieldDisplayInfo& field = kProfileFieldsToDisplay[i];

    if (field.autofillType == autofill::NAME_HONORIFIC_PREFIX &&
        !base::FeatureList::IsEnabled(
            autofill::features::kAutofillEnableSupportForHonorificPrefixes)) {
      continue;
    }

    AutofillEditItem* item =
        [[AutofillEditItem alloc] initWithType:ItemTypeField];
    item.fieldNameLabelText = l10n_util::GetNSString(field.displayStringID);
    item.textFieldValue = base::SysUTF16ToNSString(_autofillProfile.GetInfo(
        autofill::AutofillType(field.autofillType), locale));
    item.autofillUIType = AutofillUITypeFromAutofillType(field.autofillType);
    item.textFieldEnabled = self.tableView.editing;
    item.hideIcon = !self.tableView.editing;
    item.autoCapitalizationType = field.autoCapitalizationType;
    item.returnKeyType = field.returnKeyType;
    item.keyboardType = field.keyboardType;
    [model addItem:item toSectionWithIdentifier:SectionIdentifierFields];
  }
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];

  cell.selectionStyle = UITableViewCellSelectionStyleNone;

  TableViewTextEditCell* textFieldCell =
      base::mac::ObjCCastStrict<TableViewTextEditCell>(cell);
  textFieldCell.accessibilityIdentifier = textFieldCell.textLabel.text;
  textFieldCell.textField.delegate = self;
  return textFieldCell;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  if (self.tableView.editing) {
    UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:indexPath];
    TableViewTextEditCell* textFieldCell =
        base::mac::ObjCCastStrict<TableViewTextEditCell>(cell);
    [textFieldCell.textField becomeFirstResponder];
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

@end
