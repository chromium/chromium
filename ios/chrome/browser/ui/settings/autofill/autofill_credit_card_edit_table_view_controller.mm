// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill/autofill_credit_card_edit_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/feature_list.h"
#import "base/format_macros.h"
#import "base/ios/block_types.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/autofill_data_util.h"
#import "components/autofill/core/browser/data_model/credit_card.h"
#import "components/autofill/core/browser/field_types.h"
#import "components/autofill/core/browser/payments/payments_service_url.h"
#import "components/autofill/core/browser/payments_data_manager.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/autofill/core/common/credit_card_network_identifiers.h"
#import "components/autofill/core/common/credit_card_number_validation.h"
#import "components/autofill/ios/browser/credit_card_util.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_credit_card_ui_type.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_credit_card_util.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_ui_type_util.h"
#import "ios/chrome/browser/autofill/ui_bundled/cells/autofill_credit_card_edit_item.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item_delegate.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_settings_constants.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {
using ::AutofillTypeFromAutofillUITypeForCard;

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierFields = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeCardholderName = kItemTypeEnumZero,
  ItemTypeCardNumber,
  ItemTypeExpirationMonth,
  ItemTypeExpirationYear,
  ItemTypeNickname,
};

}  // namespace

@interface AutofillCreditCardEditTableViewController () <
    TableViewTextEditItemDelegate>
@end

@implementation AutofillCreditCardEditTableViewController {
  raw_ptr<autofill::PersonalDataManager> _personalDataManager;  // weak
  autofill::CreditCard _creditCard;
}

#pragma mark - Initialization

- (instancetype)initWithCreditCard:(const autofill::CreditCard&)creditCard
               personalDataManager:(autofill::PersonalDataManager*)dataManager {
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    DCHECK(dataManager);

    _personalDataManager = dataManager;
    _creditCard = creditCard;

    [self setTitle:l10n_util::GetNSString(IDS_IOS_AUTOFILL_EDIT_CREDIT_CARD)];
  }

  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.tableView.allowsSelectionDuringEditing = YES;
  self.tableView.accessibilityIdentifier = kAutofillCreditCardEditTableViewId;
  [self loadModel];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  SettingsNavigationController* navigationController =
      base::apple::ObjCCast<SettingsNavigationController>(
          self.navigationController);
  if (!navigationController) {
    return;
  }

  // Add a "Done" button to the navigation bar if this view controller is the
  // first in the navigation stack. This "Done" button's purpose being to
  // dismiss the presented view.
  if (navigationController.viewControllers.count > 0 &&
      navigationController.viewControllers.firstObject == self) {
    UIBarButtonItem* doneButton = [navigationController doneButton];

    // If not in edit mode, set the newly created "Done" button as the left bar
    // button item. Otherwise, don't override the "Cancel" button that's shown
    // when in edit mode.
    if (!self.tableView.editing) {
      self.navigationItem.leftBarButtonItem = doneButton;
    }

    // Set `customLeftBarButtonItem` with the "Done" button, so that it'll be
    // used as the left bar button item when exiting edit mode.
    self.customLeftBarButtonItem = doneButton;
  }
}

#pragma mark - SettingsRootTableViewController

- (void)editButtonPressed {
  // Check if the card should be edited from the Payments web page.
  if ([AutofillCreditCardUtil shouldEditCardFromPaymentsWebPage:_creditCard]) {
    GURL paymentsURL =
        autofill::payments::GetManageInstrumentUrl(_creditCard.instrument_id());
    OpenNewTabCommand* command =
        [OpenNewTabCommand commandWithURLFromChrome:paymentsURL];
    [self.applicationHandler closeSettingsUIAndOpenURL:command];

    return;
  }

  [super editButtonPressed];

  if (!self.tableView.editing) {
    TableViewModel* model = self.tableViewModel;
    NSInteger itemCount =
        [model numberOfItemsInSection:
                   [model sectionForSectionIdentifier:SectionIdentifierFields]];

    // Reads the values from the fields and updates the local copy of the
    // card accordingly.
    NSInteger section =
        [model sectionForSectionIdentifier:SectionIdentifierFields];
    for (NSInteger itemIndex = 0; itemIndex < itemCount; ++itemIndex) {
      NSIndexPath* path = [NSIndexPath indexPathForItem:itemIndex
                                              inSection:section];
      AutofillCreditCardEditItem* item =
          base::apple::ObjCCastStrict<AutofillCreditCardEditItem>(
              [model itemAtIndexPath:path]);
      if ([self.tableViewModel itemTypeForIndexPath:path] == ItemTypeNickname) {
        NSString* trimmedNickname = [item.textFieldValue
            stringByTrimmingCharactersInSet:
                [NSCharacterSet whitespaceAndNewlineCharacterSet]];
        _creditCard.SetNickname(base::SysNSStringToUTF16(trimmedNickname));
      } else {
        _creditCard.SetInfo(
            autofill::AutofillType(AutofillTypeFromAutofillUITypeForCard(
                item.autofillCreditCardUIType)),
            base::SysNSStringToUTF16(item.textFieldValue),
            GetApplicationContext()->GetApplicationLocale());
      }
    }

    _personalDataManager->payments_data_manager().UpdateCreditCard(_creditCard);
  }

  // Reload the model.
  [self loadModel];
  // Update the cells.
  [self reconfigureCellsForItems:
            [self.tableViewModel
                itemsInSectionWithIdentifier:SectionIdentifierFields]];
}

- (BOOL)showCancelDuringEditing {
  return YES;
}

#pragma mark - LegacyChromeTableViewController

- (void)loadModel {
  [super loadModel];
  TableViewModel<TableViewItem*>* model = self.tableViewModel;

  BOOL isEditing = self.tableView.editing;

  NSArray<AutofillCreditCardEditItem*>* editItems = @[
    [self cardNumberItem:isEditing],
    [self expirationMonthItem:isEditing],
    [self expirationYearItem:isEditing],
    [self cardholderNameItem:isEditing],
    [self nicknameItem:isEditing],
  ];

  [model addSectionWithIdentifier:SectionIdentifierFields];
  for (AutofillCreditCardEditItem* item in editItems) {
    [model addItem:item toSectionWithIdentifier:SectionIdentifierFields];
  }
}

#pragma mark - TableViewTextEditItemDelegate

- (void)tableViewItemDidBeginEditing:
    (TableViewTextEditItem*)tableViewTextEditItem {
  // No op.
}

- (void)tableViewItemDidChange:(TableViewTextEditItem*)tableViewTextEditItem {
  if ([tableViewTextEditItem
          isKindOfClass:[AutofillCreditCardEditItem class]]) {
    self.navigationItem.rightBarButtonItem.enabled = [self isValidCreditCard];

    AutofillCreditCardEditItem* item =
        (AutofillCreditCardEditItem*)tableViewTextEditItem;

    // If the user is typing in the credit card number field, update the card
    // type icon (e.g. "Visa") to reflect the number being typed.
    if (item.autofillCreditCardUIType == AutofillCreditCardUIType::kNumber) {
      const char* network = autofill::GetCardNetwork(
          base::SysNSStringToUTF16(item.textFieldValue));
      item.identifyingIcon = [self cardTypeIconFromNetwork:network];
      [self reconfigureCellsForItems:@[ item ]];
    }

    if (item.type == ItemTypeNickname) {
      NSString* trimmedText = [item.textFieldValue
          stringByTrimmingCharactersInSet:
              [NSCharacterSet whitespaceAndNewlineCharacterSet]];
      BOOL newNicknameIsValid = autofill::CreditCard::IsNicknameValid(
          base::SysNSStringToUTF16(trimmedText));
      self.navigationItem.rightBarButtonItem.enabled = newNicknameIsValid;
      [item setHasValidText:newNicknameIsValid];
      [self reconfigureCellsForItems:@[ item ]];
    }
  }
}

- (void)tableViewItemDidEndEditing:
    (TableViewTextEditItem*)tableViewTextEditItem {
  // If the table view has already been dismissed or the editing stopped
  // ignore call as the item might no longer be in the cells (crbug/1125094).
  if (!self.tableView.isEditing) {
    return;
  }

  if ([tableViewTextEditItem
          isKindOfClass:[AutofillCreditCardEditItem class]]) {
    AutofillCreditCardEditItem* item =
        (AutofillCreditCardEditItem*)tableViewTextEditItem;

    switch (item.type) {
      case ItemTypeCardNumber:
        tableViewTextEditItem.hasValidText = [AutofillCreditCardUtil
            isValidCreditCardNumber:item.textFieldValue
                           appLocal:GetApplicationContext()
                                        ->GetApplicationLocale()];
        break;
      case ItemTypeExpirationMonth:
        tableViewTextEditItem.hasValidText = [AutofillCreditCardUtil
            isValidCreditCardExpirationMonth:item.textFieldValue];
        break;
      case ItemTypeExpirationYear:
        tableViewTextEditItem.hasValidText = [AutofillCreditCardUtil
            isValidCreditCardExpirationYear:item.textFieldValue
                                   appLocal:GetApplicationContext()
                                                ->GetApplicationLocale()];
        break;
      case ItemTypeNickname:
        tableViewTextEditItem.hasValidText =
            [AutofillCreditCardUtil isValidCardNickname:item.textFieldValue];
        break;
      case ItemTypeCardholderName:
      default:
        // For the 'Name on card' textfield.
        tableViewTextEditItem.hasValidText = YES;
        break;
    }

    // Reconfigure to trigger appropiate icon change.
    [self reconfigureCellsForItems:@[ item ]];
  }
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  cell.selectionStyle = UITableViewCellSelectionStyleNone;

  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  TableViewTextEditCell* editCell =
      base::apple::ObjCCast<TableViewTextEditCell>(cell);
  editCell.textField.delegate = self;
  switch (itemType) {
    case ItemTypeCardholderName:
    case ItemTypeCardNumber:
    case ItemTypeExpirationMonth:
    case ItemTypeExpirationYear:
    case ItemTypeNickname:
      break;
    default:
      break;
  }

  return cell;
}

- (BOOL)tableView:(UITableView*)tableView
    canEditRowAtIndexPath:(NSIndexPath*)indexPath {
  // Items in this table view are not deletable, so should not be seen as
  // editable by the table view.
  return NO;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  if (self.tableView.editing) {
    UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:indexPath];
    TableViewTextEditCell* textFieldCell =
        base::apple::ObjCCastStrict<TableViewTextEditCell>(cell);
    [textFieldCell.textField becomeFirstResponder];
  }
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (BOOL)presentationControllerShouldDismiss:
    (UIPresentationController*)presentationController {
  return !self.tableView.editing;
}

#pragma mark - AutofillEditTableViewController

- (BOOL)isItemAtIndexPathTextEditCell:(NSIndexPath*)cellPath {
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:cellPath];
  switch (itemType) {
    case ItemTypeCardholderName:
    case ItemTypeCardNumber:
    case ItemTypeExpirationMonth:
    case ItemTypeExpirationYear:
    case ItemTypeNickname:
      return YES;
  }
  NOTREACHED_IN_MIGRATION();
  return NO;
}

#pragma mark - Actions

- (void)buttonTapped:(UIButton*)button {
  // TODO(crbug.com/40939195): Remove this method and button entirely; it should
  // no longer be possible to have it visible.

  // Reset the copy of the card data used for display immediately.
  _creditCard.set_record_type(
      autofill::CreditCard::RecordType::kMaskedServerCard);
  _creditCard.SetNumber(_creditCard.LastFourDigits());
  [self reloadData];
}

#pragma mark - Private

- (UIImage*)cardTypeIconFromNetwork:(const char*)network {
  if (network != autofill::kGenericCard) {
    int resourceID =
        autofill::data_util::GetPaymentRequestData(network).icon_resource_id;
    // Return the card issuer network icon.
    return NativeImage(resourceID);
  } else {
    return nil;
  }
}

- (AutofillCreditCardEditItem*)cardholderNameItem:(bool)isEditing {
  AutofillCreditCardEditItem* cardholderNameItem =
      [[AutofillCreditCardEditItem alloc] initWithType:ItemTypeCardholderName];
  cardholderNameItem.fieldNameLabelText =
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_CARDHOLDER);
  cardholderNameItem.textFieldValue = autofill::GetCreditCardName(
      _creditCard, GetApplicationContext()->GetApplicationLocale());
  cardholderNameItem.textFieldEnabled = isEditing;
  cardholderNameItem.autofillCreditCardUIType =
      AutofillCreditCardUIType::kFullName;
  cardholderNameItem.hideIcon = !isEditing;
  return cardholderNameItem;
}

- (AutofillCreditCardEditItem*)cardNumberItem:(bool)isEditing {
  AutofillCreditCardEditItem* cardNumberItem =
      [[AutofillCreditCardEditItem alloc] initWithType:ItemTypeCardNumber];
  cardNumberItem.fieldNameLabelText =
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_CARD_NUMBER);
  // Never show full card number for Wallet cards, even if copied locally.
  cardNumberItem.textFieldValue =
      autofill::IsCreditCardLocal(_creditCard)
          ? base::SysUTF16ToNSString(_creditCard.number())
          : base::SysUTF16ToNSString(_creditCard.NetworkAndLastFourDigits());
  cardNumberItem.textFieldEnabled = isEditing;
  cardNumberItem.autofillCreditCardUIType = AutofillCreditCardUIType::kNumber;
  cardNumberItem.keyboardType = UIKeyboardTypeNumberPad;
  cardNumberItem.hideIcon = !isEditing;
  cardNumberItem.delegate = self;
  // Hide credit card icon when editing.
  if (!isEditing) {
    cardNumberItem.identifyingIcon =
        [self cardTypeIconFromNetwork:_creditCard.network().c_str()];
  }
  return cardNumberItem;
}

- (AutofillCreditCardEditItem*)expirationMonthItem:(bool)isEditing {
  AutofillCreditCardEditItem* expirationMonthItem =
      [[AutofillCreditCardEditItem alloc] initWithType:ItemTypeExpirationMonth];
  expirationMonthItem.fieldNameLabelText =
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_EXP_MONTH);
  expirationMonthItem.textFieldValue =
      [NSString stringWithFormat:@"%02d", _creditCard.expiration_month()];
  expirationMonthItem.textFieldEnabled = isEditing;
  expirationMonthItem.autofillCreditCardUIType =
      AutofillCreditCardUIType::kExpMonth;
  expirationMonthItem.keyboardType = UIKeyboardTypeNumberPad;
  expirationMonthItem.hideIcon = !isEditing;
  expirationMonthItem.delegate = self;
  return expirationMonthItem;
}

- (AutofillCreditCardEditItem*)expirationYearItem:(bool)isEditing {
  // Expiration year.
  AutofillCreditCardEditItem* expirationYearItem =
      [[AutofillCreditCardEditItem alloc] initWithType:ItemTypeExpirationYear];
  expirationYearItem.fieldNameLabelText =
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_EXP_YEAR);
  expirationYearItem.textFieldValue =
      [NSString stringWithFormat:@"%04d", _creditCard.expiration_year()];
  expirationYearItem.textFieldEnabled = isEditing;
  expirationYearItem.autofillCreditCardUIType =
      AutofillCreditCardUIType::kExpYear;
  expirationYearItem.keyboardType = UIKeyboardTypeNumberPad;
  expirationYearItem.returnKeyType = UIReturnKeyDone;
  expirationYearItem.hideIcon = !isEditing;
  expirationYearItem.delegate = self;
  return expirationYearItem;
}

- (AutofillCreditCardEditItem*)nicknameItem:(bool)isEditing {
  AutofillCreditCardEditItem* nicknameItem =
      [[AutofillCreditCardEditItem alloc] initWithType:ItemTypeNickname];
  nicknameItem.fieldNameLabelText =
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_NICKNAME);
  nicknameItem.textFieldValue =
      autofill::GetCreditCardNicknameString(_creditCard);
  nicknameItem.textFieldPlaceholder =
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_DIALOG_PLACEHOLDER_NICKNAME);
  nicknameItem.textFieldEnabled = isEditing;
  nicknameItem.keyboardType = UIKeyboardTypeDefault;
  nicknameItem.hideIcon = !isEditing;
  nicknameItem.delegate = self;
  return nicknameItem;
}

// Returns YES if the data entered in the fields represent a valid credit card.
- (BOOL)isValidCreditCard {
  NSString* cardNumber = [self textfieldValueForItemType:ItemTypeCardNumber];
  NSString* expirationMonth =
      [self textfieldValueForItemType:ItemTypeExpirationMonth];
  NSString* expirationYear =
      [self textfieldValueForItemType:ItemTypeExpirationYear];
  NSString* nickname = [self textfieldValueForItemType:ItemTypeNickname];
  return [AutofillCreditCardUtil
      isValidCreditCard:cardNumber
        expirationMonth:expirationMonth
         expirationYear:expirationYear
           cardNickname:nickname
               appLocal:GetApplicationContext()->GetApplicationLocale()];
}

// Returns the value in the field corresponding to the `itemType`.
- (NSString*)textfieldValueForItemType:(ItemType)itemType {
  NSIndexPath* indexPath =
      [self.tableViewModel indexPathForItemType:itemType
                              sectionIdentifier:SectionIdentifierFields];
  AutofillCreditCardEditItem* item =
      base::apple::ObjCCastStrict<AutofillCreditCardEditItem>(
          [self.tableViewModel itemAtIndexPath:indexPath]);
  return item.textFieldValue;
}

@end
