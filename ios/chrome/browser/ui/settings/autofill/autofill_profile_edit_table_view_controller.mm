// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill/autofill_profile_edit_table_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/stl_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/payments/payments_service_url.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "ios/chrome/browser/application_context.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type_util.h"
#import "ios/chrome/browser/ui/autofill/cells/autofill_edit_item.h"
#include "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_constants.h"
#import "ios/chrome/browser/ui/table_view/table_view_model.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

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

struct AutofillFieldDisplayInfo {
  autofill::ServerFieldType autofillType;
  int displayStringID;
  UIReturnKeyType returnKeyType;
  UIKeyboardType keyboardType;
  UITextAutocapitalizationType autoCapitalizationType;
};

static const AutofillFieldDisplayInfo kFieldsToDisplay[] = {
    {autofill::NAME_FULL, IDS_IOS_AUTOFILL_FULLNAME, UIReturnKeyNext,
     UIKeyboardTypeDefault, UITextAutocapitalizationTypeSentences},
    {autofill::COMPANY_NAME, IDS_IOS_AUTOFILL_COMPANY_NAME, UIReturnKeyNext,
     UIKeyboardTypeDefault, UITextAutocapitalizationTypeSentences},
    {autofill::ADDRESS_HOME_LINE1, IDS_IOS_AUTOFILL_ADDRESS1, UIReturnKeyNext,
     UIKeyboardTypeDefault, UITextAutocapitalizationTypeSentences},
    {autofill::ADDRESS_HOME_LINE2, IDS_IOS_AUTOFILL_ADDRESS2, UIReturnKeyNext,
     UIKeyboardTypeDefault, UITextAutocapitalizationTypeSentences},
    {autofill::ADDRESS_HOME_CITY, IDS_IOS_AUTOFILL_CITY, UIReturnKeyNext,
     UIKeyboardTypeDefault, UITextAutocapitalizationTypeSentences},
    {autofill::ADDRESS_HOME_STATE, IDS_IOS_AUTOFILL_STATE, UIReturnKeyNext,
     UIKeyboardTypeDefault, UITextAutocapitalizationTypeSentences},
    {autofill::ADDRESS_HOME_ZIP, IDS_IOS_AUTOFILL_ZIP, UIReturnKeyNext,
     UIKeyboardTypeDefault, UITextAutocapitalizationTypeAllCharacters},
    {autofill::ADDRESS_HOME_COUNTRY, IDS_IOS_AUTOFILL_COUNTRY, UIReturnKeyNext,
     UIKeyboardTypeDefault, UITextAutocapitalizationTypeSentences},
    {autofill::PHONE_HOME_WHOLE_NUMBER, IDS_IOS_AUTOFILL_PHONE, UIReturnKeyNext,
     UIKeyboardTypePhonePad, UITextAutocapitalizationTypeSentences},
    {autofill::EMAIL_ADDRESS, IDS_IOS_AUTOFILL_EMAIL, UIReturnKeyDone,
     UIKeyboardTypeEmailAddress, UITextAutocapitalizationTypeNone}};

}  // namespace

@interface AutofillProfileEditTableViewController ()

// Initializes a AutofillProfileEditTableViewController with |profile| and
// |dataManager|.
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

  UITableViewStyle style = base::FeatureList::IsEnabled(kSettingsRefresh)
                               ? UITableViewStylePlain
                               : UITableViewStyleGrouped;
  self = [super initWithTableViewStyle:style
                           appBarStyle:ChromeTableViewControllerStyleNoAppBar];
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
  // In the case of server profiles, open the Payments editing page instead.
  if (_autofillProfile.record_type() ==
      autofill::AutofillProfile::SERVER_PROFILE) {
    GURL paymentsURL = autofill::payments::GetManageAddressesUrl();
    OpenNewTabCommand* command =
        [OpenNewTabCommand commandWithURLFromChrome:paymentsURL];
    [self.dispatcher closeSettingsUIAndOpenURL:command];

    // Don't call [super editButtonPressed] because edit mode is not actually
    // entered in this case.
    return;
  }

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
      if (item.autofillUIType == AutofillUITypeProfileHomeAddressCountry) {
        _autofillProfile.SetInfo(
            autofill::AutofillType(serverFieldType),
            base::SysNSStringToUTF16(item.textFieldValue),
            GetApplicationContext()->GetApplicationLocale());
      } else {
        _autofillProfile.SetRawInfo(
            serverFieldType, base::SysNSStringToUTF16(item.textFieldValue));
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
  for (size_t i = 0; i < base::size(kFieldsToDisplay); ++i) {
    const AutofillFieldDisplayInfo& field = kFieldsToDisplay[i];

    if ((field.autofillType == autofill::COMPANY_NAME) &&
        !base::FeatureList::IsEnabled(
            autofill::features::kAutofillEnableCompanyName)) {
      continue;
    }

    AutofillEditItem* item =
        [[AutofillEditItem alloc] initWithType:ItemTypeField];
    item.textFieldName = l10n_util::GetNSString(field.displayStringID);
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
