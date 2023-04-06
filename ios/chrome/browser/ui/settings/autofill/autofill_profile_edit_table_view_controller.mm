// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill/autofill_profile_edit_table_view_controller.h"

#import "base/mac/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_model/autofill_profile.h"
#import "components/autofill/core/browser/field_types.h"
#import "components/autofill/core/common/autofill_features.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_attributed_string_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_multi_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item_delegate.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type_util.h"
#import "ios/chrome/browser/ui/autofill/cells/autofill_edit_item.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_constants.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_profile_edit_table_view_controller_delegate.h"
#import "ios/chrome/browser/ui/settings/autofill/cells/country_item.h"
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
  SectionIdentifierErrorFooter,
  SectionIdentifierFooter
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeHonorificPrefix = kItemTypeEnumZero,
  ItemTypeCompanyName,
  ItemTypeFullName,
  ItemTypeLine1,
  ItemTypeLine2,
  ItemTypeCity,
  ItemTypeState,
  ItemTypeZip,
  ItemTypeCountry,
  ItemTypePhoneNumber,
  ItemTypeEmailAddress,
  ItemTypeError,
  ItemTypeFooter
};

// A constant to separate the error and the footer text.
const CGFloat kLineSpacingBetweenErrorAndFooter = 12.0f;

}  // namespace

@interface AutofillProfileEditTableViewController () <
    TableViewTextEditItemDelegate>

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
@property(nonatomic, assign) BOOL line1Required;
@property(nonatomic, assign) BOOL cityRequired;
@property(nonatomic, assign) BOOL stateRequired;
@property(nonatomic, assign) BOOL zipRequired;

// YES, if the profile's source is autofill::AutofillProfile::Source::kAccount.
@property(nonatomic, assign) BOOL accountProfile;

// Returns YES if the feature
// `autofill::features::kAutofillAccountProfilesUnionView` is enabled.
@property(nonatomic, assign) BOOL autofillAccountProfilesUnionViewEnabled;

@end

@implementation AutofillProfileEditTableViewController {
  NSString* _userEmail;
}

#pragma mark - Initialization

- (instancetype)initWithDelegate:
                    (id<AutofillProfileEditTableViewControllerDelegate>)delegate
                       userEmail:(NSString*)userEmail {
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _delegate = delegate;
    _userEmail = userEmail;
    _errorSectionPresented = NO;
    _accountProfile = NO;
    _autofillAccountProfilesUnionViewEnabled = base::FeatureList::IsEnabled(
        autofill::features::kAutofillAccountProfilesUnionView);
    _requiredFieldsWithEmptyValue = [[NSMutableSet<NSString*> alloc] init];

    [self setTitle:l10n_util::GetNSString(IDS_IOS_AUTOFILL_EDIT_ADDRESS)];
  }

  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.tableView.allowsSelectionDuringEditing = YES;
  self.tableView.accessibilityIdentifier = kAutofillProfileEditTableViewId;

  [self loadModel];
}

- (void)viewDidDisappear:(BOOL)animated {
  [self.delegate viewDidDisappear];
  [super viewDidDisappear:animated];
}

- (BOOL)isItemAtIndexPathTextEditCell:(NSIndexPath*)cellPath {
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:cellPath];
  switch (static_cast<ItemType>(itemType)) {
    case ItemTypeHonorificPrefix:
    case ItemTypeCompanyName:
    case ItemTypeFullName:
    case ItemTypeLine1:
    case ItemTypeLine2:
    case ItemTypeCity:
    case ItemTypeState:
    case ItemTypeZip:
    case ItemTypePhoneNumber:
    case ItemTypeEmailAddress:
      return YES;
    case ItemTypeCountry:
      return !self.autofillAccountProfilesUnionViewEnabled;
    case ItemTypeError:
    case ItemTypeFooter:
      return NO;
  }
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

      if (itemType == ItemTypeCountry &&
          self.autofillAccountProfilesUnionViewEnabled) {
        [self.delegate
            updateProfileMetadataWithValue:self.homeAddressCountry
                         forAutofillUIType:
                             AutofillUITypeProfileHomeAddressCountry];
        continue;
      }

      AutofillEditItem* item = base::mac::ObjCCastStrict<AutofillEditItem>(
          [model itemAtIndexPath:path]);
      [self.delegate updateProfileMetadataWithValue:item.textFieldValue
                                  forAutofillUIType:item.autofillUIType];
    }

    [self.delegate didEditAutofillProfile];
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

    if (self.autofillAccountProfilesUnionViewEnabled &&
        AutofillUITypeFromAutofillType(field.autofillType) ==
            AutofillUITypeProfileHomeAddressCountry) {
      [model addItem:[self countryItem]
          toSectionWithIdentifier:SectionIdentifierFields];
    } else {
      [model addItem:[self autofillEditItemFromField:field]
          toSectionWithIdentifier:SectionIdentifierFields];
    }
  }

  if (self.autofillAccountProfilesUnionViewEnabled && self.accountProfile &&
      _userEmail != nil) {
    [model addSectionWithIdentifier:SectionIdentifierFooter];
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

  if (self.autofillAccountProfilesUnionViewEnabled &&
      itemType == ItemTypeCountry) {
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
    if (self.autofillAccountProfilesUnionViewEnabled &&
        itemType == ItemTypeCountry) {
      [self.delegate willSelectCountryWithCurrentlySelectedCountry:
                         self.homeAddressCountry];
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
      sectionIdentifier == SectionIdentifierErrorFooter) {
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
  if (self.autofillAccountProfilesUnionViewEnabled && self.accountProfile) {
    [self computeErrorIfRequiredTextField:tableViewItem];
    [self updateDoneButtonStatus];
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
  DCHECK(self.autofillAccountProfilesUnionViewEnabled);
  self.homeAddressCountry = country;

  [self.requiredFieldsWithEmptyValue removeAllObjects];
  for (TableViewItem* item in [self.tableViewModel
           itemsInSectionWithIdentifier:SectionIdentifierFields]) {
    if (item.type == ItemTypeCountry) {
      TableViewMultiDetailTextItem* multiDetailTextItem =
          base::mac::ObjCCastStrict<TableViewMultiDetailTextItem>(item);
      multiDetailTextItem.trailingDetailText = self.homeAddressCountry;
    } else {
      TableViewTextEditItem* tableViewTextEditItem =
          base::mac::ObjCCastStrict<TableViewTextEditItem>(item);
      [self computeErrorIfRequiredTextField:tableViewTextEditItem];
    }
    [self reconfigureCellsForItems:@[ item ]];
  }

  [self updateDoneButtonStatus];
}

#pragma mark - Helper Methods

// Returns `autofill::ServerFieldType` corresponding to the `itemType`.
- (autofill::ServerFieldType)serverFieldTypeCorrespondingToRequiredItemType:
    (ItemType)itemType {
  switch (itemType) {
    case ItemTypeLine1:
      return autofill::ADDRESS_HOME_LINE1;
    case ItemTypeCity:
      return autofill::ADDRESS_HOME_CITY;
    case ItemTypeState:
      return autofill::ADDRESS_HOME_STATE;
    case ItemTypeZip:
      return autofill::ADDRESS_HOME_ZIP;
    case ItemTypeHonorificPrefix:
    case ItemTypeCompanyName:
    case ItemTypeFullName:
    case ItemTypeLine2:
    case ItemTypePhoneNumber:
    case ItemTypeEmailAddress:
    case ItemTypeCountry:
    case ItemTypeError:
    case ItemTypeFooter:
      break;
  }
  NOTREACHED();
  return autofill::UNKNOWN_TYPE;
}

// Returns the label corresponding to the item type for a required field.
- (NSString*)labelCorrespondingToRequiredItemType:(ItemType)itemType {
  switch (itemType) {
    case ItemTypeLine1:
      return l10n_util::GetNSString(IDS_IOS_AUTOFILL_ADDRESS1);
    case ItemTypeCity:
      return l10n_util::GetNSString(IDS_IOS_AUTOFILL_CITY);
    case ItemTypeState:
      return l10n_util::GetNSString(IDS_IOS_AUTOFILL_STATE);
    case ItemTypeZip:
      return l10n_util::GetNSString(IDS_IOS_AUTOFILL_ZIP);
    case ItemTypeHonorificPrefix:
    case ItemTypeCompanyName:
    case ItemTypeFullName:
    case ItemTypeLine2:
    case ItemTypePhoneNumber:
    case ItemTypeEmailAddress:
    case ItemTypeCountry:
    case ItemTypeError:
    case ItemTypeFooter:
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
- (ItemType)itemTypeForAutofillUIType:(AutofillUIType)autofillUIType {
  switch (autofillUIType) {
    case AutofillUITypeProfileHonorificPrefix:
      return ItemTypeHonorificPrefix;
    case AutofillUITypeProfileCompanyName:
      return ItemTypeCompanyName;
    case AutofillUITypeProfileFullName:
      return ItemTypeFullName;
    case AutofillUITypeProfileHomeAddressLine1:
      return ItemTypeLine1;
    case AutofillUITypeProfileHomeAddressLine2:
      return ItemTypeLine2;
    case AutofillUITypeProfileHomeAddressCity:
      return ItemTypeCity;
    case AutofillUITypeProfileHomeAddressState:
      return ItemTypeState;
    case AutofillUITypeProfileHomeAddressZip:
      return ItemTypeZip;
    case AutofillUITypeProfileHomeAddressCountry:
      return ItemTypeCountry;
    case AutofillUITypeProfileHomePhoneWholeNumber:
      return ItemTypePhoneNumber;
    case AutofillUITypeProfileEmailAddress:
      return ItemTypeEmailAddress;
    default:
      break;
  }
  NOTREACHED();
  return ItemTypeError;
}

#pragma mark - Private

// Returns true if the itemType belongs to a required field.
- (BOOL)isItemTypeRequiredField:(ItemType)itemType {
  switch (itemType) {
    case ItemTypeLine1:
      return self.line1Required;
    case ItemTypeCity:
      return self.cityRequired;
    case ItemTypeState:
      return self.stateRequired;
    case ItemTypeZip:
      return self.zipRequired;
    case ItemTypeHonorificPrefix:
    case ItemTypeCompanyName:
    case ItemTypeFullName:
    case ItemTypeLine2:
    case ItemTypePhoneNumber:
    case ItemTypeEmailAddress:
    case ItemTypeCountry:
    case ItemTypeError:
    case ItemTypeFooter:
      break;
  }
  return NO;
}

// Computes whether the `tableViewItem` is a required field and empty.
- (void)computeErrorIfRequiredTextField:(TableViewTextEditItem*)tableViewItem {
  ItemType itemType = static_cast<ItemType>(tableViewItem.type);
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
- (BOOL)requiredFieldWasEmptyOnProfileLoadForItemType:(ItemType)itemType {
  DCHECK([self isItemTypeRequiredField:itemType]);

  return [self.delegate
      fieldValueEmptyOnProfileLoadForType:
          [self serverFieldTypeCorrespondingToRequiredItemType:itemType]];
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

// If the error status has changed, displays the footer accordingly.
- (void)changeFooterStatusToRemoveSection:(SectionIdentifier)removeSection
                               addSection:(SectionIdentifier)addSection {
  TableViewModel* model = self.tableViewModel;
  [self
      performBatchTableViewUpdates:^{
        [self removeSectionWithIdentifier:removeSection
                         withRowAnimation:UITableViewRowAnimationTop];
        NSUInteger fieldsSectionIndex =
            [model sectionForSectionIdentifier:SectionIdentifierFields];
        [model insertSectionWithIdentifier:addSection
                                   atIndex:fieldsSectionIndex + 1];
        [self.tableView
              insertSections:[NSIndexSet
                                 indexSetWithIndex:fieldsSectionIndex + 1]
            withRowAnimation:UITableViewRowAnimationTop];
        [self.tableViewModel setFooter:(([self.requiredFieldsWithEmptyValue
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
  BOOL shouldShowError = ([self.requiredFieldsWithEmptyValue count] > 0);
  self.navigationItem.rightBarButtonItem.enabled = !shouldShowError;
  if (shouldShowError != self.errorSectionPresented) {
    SectionIdentifier addSection = shouldShowError
                                       ? SectionIdentifierErrorFooter
                                       : SectionIdentifierFooter;
    SectionIdentifier removeSection = shouldShowError
                                          ? SectionIdentifierFooter
                                          : SectionIdentifierErrorFooter;
    [self changeFooterStatusToRemoveSection:removeSection
                                 addSection:addSection];
    self.errorSectionPresented = shouldShowError;
  } else if (shouldShowError && [self shouldChangeErrorMessage]) {
    [self changeFooterStatusToRemoveSection:SectionIdentifierErrorFooter
                                 addSection:SectionIdentifierErrorFooter];
  }
}

// Returns YES, if the error message needs to be changed. This happens when
// there are multiple required fields that become empty.
- (BOOL)shouldChangeErrorMessage {
  TableViewHeaderFooterItem* currentFooter = [self.tableViewModel
      footerForSectionWithIdentifier:SectionIdentifierErrorFooter];
  TableViewAttributedStringHeaderFooterItem* attributedFooterItem =
      base::mac::ObjCCastStrict<TableViewAttributedStringHeaderFooterItem>(
          currentFooter);
  NSAttributedString* newFooter = [self errorAndFooterMessage];
  return ![attributedFooterItem.attributedString
      isEqualToAttributedString:newFooter];
}

// Returns the error message item as a footer.
- (TableViewAttributedStringHeaderFooterItem*)errorMessageItem {
  TableViewAttributedStringHeaderFooterItem* item =
      [[TableViewAttributedStringHeaderFooterItem alloc]
          initWithType:ItemTypeError];
  [item setAttributedString:[self errorAndFooterMessage]];
  return item;
}

// Returns the footer message.
- (NSString*)footerMessage {
  return l10n_util::GetNSStringF(
      IDS_IOS_SETTINGS_AUTOFILL_ACCOUNT_ADDRESS_FOOTER_TEXT,
      base::SysNSStringToUTF16(_userEmail));
}

// Returns the error message combined with footer.
- (NSAttributedString*)errorAndFooterMessage {
  DCHECK([self.requiredFieldsWithEmptyValue count] > 0);
  NSString* error = l10n_util::GetPluralNSStringF(
      IDS_IOS_SETTINGS_EDIT_AUTOFILL_ADDRESS_REQUIREMENT_ERROR,
      (int)[self.requiredFieldsWithEmptyValue count]);

  NSString* finalErrorString = error;
  if (_userEmail != nil) {
    finalErrorString =
        [NSString stringWithFormat:@"%@\n%@", error, [self footerMessage]];
  }

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

// Creates and returns the `TableViewLinkHeaderFooterItem` footer item.
- (TableViewLinkHeaderFooterItem*)footerItem {
  TableViewLinkHeaderFooterItem* item =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeFooter];
  item.text = [self footerMessage];
  return item;
}

- (AutofillEditItem*)autofillEditItemFromField:
    (const AutofillProfileFieldDisplayInfo&)field {
  AutofillUIType autofillUIType =
      AutofillUITypeFromAutofillType(field.autofillType);
  AutofillEditItem* item = [[AutofillEditItem alloc]
      initWithType:[self itemTypeForAutofillUIType:autofillUIType]];
  item.fieldNameLabelText = l10n_util::GetNSString(field.displayStringID);
  item.textFieldValue = [self valueForAutofillUIType:autofillUIType];
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
  item.trailingDetailText = self.homeAddressCountry;
  item.trailingDetailTextColor = [UIColor colorNamed:kTextPrimaryColor];
  if (self.tableView.editing) {
    item.editingAccessoryType = UITableViewCellAccessoryDisclosureIndicator;
  }
  return item;
}

@end
