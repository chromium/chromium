// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill_credit_card_edit_collection_view_controller.h"

#include "base/format_macros.h"
#import "base/ios/block_types.h"
#import "base/mac/foundation_util.h"
#include "base/mac/scoped_block.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/payments/payments_service_url.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#import "components/autofill/ios/browser/credit_card_util.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/application_context.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type_util.h"
#import "ios/chrome/browser/ui/autofill/cells/autofill_edit_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/settings/autofill_edit_collection_view_controller+protected.h"
#import "ios/chrome/browser/ui/settings/cells/copied_to_chrome_item.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using ::AutofillTypeFromAutofillUIType;

NSString* const kAutofillCreditCardEditCollectionViewId =
    @"kAutofillCreditCardEditCollectionViewId";

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierFields = kSectionIdentifierEnumZero,
  SectionIdentifierCopiedToChrome,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeCardholderName = kItemTypeEnumZero,
  ItemTypeCardNumber,
  ItemTypeExpirationMonth,
  ItemTypeExpirationYear,
  ItemTypeCopiedToChrome,
};

}  // namespace

@implementation AutofillCreditCardEditCollectionViewController {
  autofill::PersonalDataManager* _personalDataManager;  // weak
  autofill::CreditCard _creditCard;
}

#pragma mark - Initialization

- (instancetype)initWithCreditCard:(const autofill::CreditCard&)creditCard
               personalDataManager:(autofill::PersonalDataManager*)dataManager {
  UICollectionViewLayout* layout = [[MDCCollectionViewFlowLayout alloc] init];
  self =
      [super initWithLayout:layout style:CollectionViewControllerStyleAppBar];
  if (self) {
    DCHECK(dataManager);

    _personalDataManager = dataManager;
    _creditCard = creditCard;

    [self setCollectionViewAccessibilityIdentifier:
              kAutofillCreditCardEditCollectionViewId];
    [self setTitle:l10n_util::GetNSString(IDS_IOS_AUTOFILL_EDIT_CREDIT_CARD)];
    // TODO(crbug.com/764578): -loadModel should not be called from
    // initializer. A possible fix is to move this call to -viewDidLoad.
    [self loadModel];
  }

  return self;
}

#pragma mark - SettingsRootCollectionViewController

- (void)editButtonPressed {
  // In the case of server cards, open the Payments editing page instead.
  if (_creditCard.record_type() == autofill::CreditCard::FULL_SERVER_CARD ||
      _creditCard.record_type() == autofill::CreditCard::MASKED_SERVER_CARD) {
    GURL paymentsURL = autofill::payments::GetManageInstrumentsUrl(0);
    OpenNewTabCommand* command =
        [OpenNewTabCommand commandWithURLFromChrome:paymentsURL];
    [self.dispatcher closeSettingsUIAndOpenURL:command];

    // Don't call [super editButtonPressed] because edit mode is not actually
    // entered in this case.
    return;
  }

  [super editButtonPressed];

  if (!self.editor.editing) {
    CollectionViewModel* model = self.collectionViewModel;
    NSInteger itemCount =
        [model numberOfItemsInSection:
                   [model sectionForSectionIdentifier:SectionIdentifierFields]];

    // Reads the values from the fields and updates the local copy of the
    // card accordingly.
    NSInteger section =
        [model sectionForSectionIdentifier:SectionIdentifierFields];
    for (NSInteger itemIndex = 0; itemIndex < itemCount; ++itemIndex) {
      NSIndexPath* path =
          [NSIndexPath indexPathForItem:itemIndex inSection:section];
      AutofillEditItem* item = base::mac::ObjCCastStrict<AutofillEditItem>(
          [model itemAtIndexPath:path]);
      _creditCard.SetInfo(autofill::AutofillType(AutofillTypeFromAutofillUIType(
                              item.autofillUIType)),
                          base::SysNSStringToUTF16(item.textFieldValue),
                          GetApplicationContext()->GetApplicationLocale());
    }

    _personalDataManager->UpdateCreditCard(_creditCard);
  }

  // Reload the model.
  [self loadModel];
  // Update the cells.
  [self reconfigureCellsForItems:
            [self.collectionViewModel
                itemsInSectionWithIdentifier:SectionIdentifierFields]];
}

- (void)loadModel {
  [super loadModel];
  CollectionViewModel* model = self.collectionViewModel;

  BOOL isEditing = self.editor.editing;

  [model addSectionWithIdentifier:SectionIdentifierFields];
  AutofillEditItem* cardholderNameitem =
      [[AutofillEditItem alloc] initWithType:ItemTypeCardholderName];
  cardholderNameitem.cellStyle = CollectionViewCellStyle::kUIKit;
  cardholderNameitem.textFieldName =
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_CARDHOLDER);
  cardholderNameitem.textFieldValue = autofill::GetCreditCardName(
      _creditCard, GetApplicationContext()->GetApplicationLocale());
  cardholderNameitem.textFieldEnabled = isEditing;
  cardholderNameitem.autofillUIType = AutofillUITypeCreditCardHolderFullName;
  [model addItem:cardholderNameitem
      toSectionWithIdentifier:SectionIdentifierFields];

  // Card number (PAN).
  AutofillEditItem* cardNumberItem =
      [[AutofillEditItem alloc] initWithType:ItemTypeCardNumber];
  cardNumberItem.cellStyle = CollectionViewCellStyle::kUIKit;
  cardNumberItem.textFieldName =
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_CARD_NUMBER);
  // Never show full card number for Wallet cards, even if copied locally.
  cardNumberItem.textFieldValue =
      autofill::IsCreditCardLocal(_creditCard)
          ? base::SysUTF16ToNSString(_creditCard.number())
          : base::SysUTF16ToNSString(
                _creditCard.NetworkOrBankNameAndLastFourDigits());
  cardNumberItem.textFieldEnabled = isEditing;
  cardNumberItem.autofillUIType = AutofillUITypeCreditCardNumber;
  cardNumberItem.keyboardType = UIKeyboardTypeNumberPad;
  cardNumberItem.identifyingIcon =
      [self cardTypeIconFromNetwork:_creditCard.network().c_str()];
  [model addItem:cardNumberItem
      toSectionWithIdentifier:SectionIdentifierFields];

  // Expiration month.
  AutofillEditItem* expirationMonthItem =
      [[AutofillEditItem alloc] initWithType:ItemTypeExpirationMonth];
  expirationMonthItem.cellStyle = CollectionViewCellStyle::kUIKit;
  expirationMonthItem.textFieldName =
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_EXP_MONTH);
  expirationMonthItem.textFieldValue =
      [NSString stringWithFormat:@"%02d", _creditCard.expiration_month()];
  expirationMonthItem.textFieldEnabled = isEditing;
  expirationMonthItem.autofillUIType = AutofillUITypeCreditCardExpMonth;
  expirationMonthItem.keyboardType = UIKeyboardTypeNumberPad;
  [model addItem:expirationMonthItem
      toSectionWithIdentifier:SectionIdentifierFields];

  // Expiration year.
  AutofillEditItem* expirationYearItem =
      [[AutofillEditItem alloc] initWithType:ItemTypeExpirationYear];
  expirationYearItem.cellStyle = CollectionViewCellStyle::kUIKit;
  expirationYearItem.textFieldName =
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_EXP_YEAR);
  expirationYearItem.textFieldValue =
      [NSString stringWithFormat:@"%04d", _creditCard.expiration_year()];
  expirationYearItem.textFieldEnabled = isEditing;
  expirationYearItem.autofillUIType = AutofillUITypeCreditCardExpYear;
  expirationYearItem.keyboardType = UIKeyboardTypeNumberPad;
  expirationYearItem.returnKeyType = UIReturnKeyDone;
  [model addItem:expirationYearItem
      toSectionWithIdentifier:SectionIdentifierFields];

  if (_creditCard.record_type() == autofill::CreditCard::FULL_SERVER_CARD) {
    // Add CopiedToChrome cell in its own section.
    [model addSectionWithIdentifier:SectionIdentifierCopiedToChrome];
    CopiedToChromeItem* copiedToChromeItem =
        [[CopiedToChromeItem alloc] initWithType:ItemTypeCopiedToChrome];
    [model addItem:copiedToChromeItem
        toSectionWithIdentifier:SectionIdentifierCopiedToChrome];
  }
}

#pragma mark - UITextFieldDelegate

// This method is called as the text is being typed in, pasted, or deleted. Asks
// the delegate if the text should be changed. Should always return YES. During
// typing/pasting text, |newText| contains one or more new characters. When user
// deletes text, |newText| is empty. |range| is the range of characters to be
// replaced.
- (BOOL)textField:(UITextField*)textField
    shouldChangeCharactersInRange:(NSRange)range
                replacementString:(NSString*)newText {
  // Find the respective item for the text field.
  NSIndexPath* indexPath = [self indexPathForCurrentTextField];
  DCHECK(indexPath);
  AutofillEditItem* item = base::mac::ObjCCastStrict<AutofillEditItem>(
      [self.collectionViewModel itemAtIndexPath:indexPath]);

  // If the user is typing in the credit card number field, update the card type
  // icon (e.g. "Visa") to reflect the number being typed.
  if (item.autofillUIType == AutofillUITypeCreditCardNumber) {
    // Obtain the text being typed.
    NSString* updatedText =
        [textField.text stringByReplacingCharactersInRange:range
                                                withString:newText];
    const char* network = autofill::CreditCard::GetCardNetwork(
        base::SysNSStringToUTF16(updatedText));
    item.identifyingIcon = [self cardTypeIconFromNetwork:network];
    // Update the cell.
    [self reconfigureCellsForItems:@[ item ]];
  }

  return YES;
}

#pragma mark - MDCCollectionViewEditingDelegate

- (BOOL)collectionViewAllowsEditing:(UICollectionView*)collectionView {
  // The collection view needs to allow editing in order to respond to the Edit
  // button.
  return YES;
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    canEditItemAtIndexPath:(NSIndexPath*)indexPath {
  // Items in this collection view are not deletable, so should not be seen
  // as editable by the collection view.
  return NO;
}

#pragma mark - UICollectionViewDataSource

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  UICollectionViewCell* cell =
      [super collectionView:collectionView cellForItemAtIndexPath:indexPath];

  NSInteger itemType =
      [self.collectionViewModel itemTypeForIndexPath:indexPath];
  AutofillEditCell* textFieldCell = base::mac::ObjCCast<AutofillEditCell>(cell);
  textFieldCell.textField.delegate = self;
  switch (itemType) {
    case ItemTypeCardholderName:
    case ItemTypeCardNumber:
    case ItemTypeExpirationMonth:
    case ItemTypeExpirationYear:
      break;
    case ItemTypeCopiedToChrome: {
      CopiedToChromeCell* copiedToChromeCell =
          base::mac::ObjCCastStrict<CopiedToChromeCell>(cell);
      [copiedToChromeCell.button addTarget:self
                                    action:@selector(buttonTapped:)
                          forControlEvents:UIControlEventTouchUpInside];
      break;
    }
    default:
      break;
  }

  return cell;
}

#pragma mark - Actions

- (void)buttonTapped:(UIButton*)button {
  _personalDataManager->ResetFullServerCard(_creditCard.guid());

  // Reset the copy of the card data used for display immediately.
  _creditCard.set_record_type(autofill::CreditCard::MASKED_SERVER_CARD);
  _creditCard.SetNumber(_creditCard.LastFourDigits());
  [self reloadData];
}

#pragma mark - Helper Methods

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

@end
