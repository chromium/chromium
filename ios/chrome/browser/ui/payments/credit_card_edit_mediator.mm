// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/credit_card_edit_mediator.h"

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_clock.h"
#import "components/autofill/ios/browser/credit_card_util.h"
#include "components/payments/core/autofill_card_validation.h"
#include "components/payments/core/payment_request_data_util.h"
#include "components/payments/core/strings_util.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/payments/payment_request.h"
#import "ios/chrome/browser/payments/payment_request_util.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type_util.h"
#import "ios/chrome/browser/ui/payments/cells/accepted_payment_methods_item.h"
#import "ios/chrome/browser/ui/payments/cells/payment_method_item.h"
#import "ios/chrome/browser/ui/payments/payment_request_edit_consumer.h"
#import "ios/chrome/browser/ui/payments/payment_request_editor_field.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using ::autofill::data_util::GetIssuerNetworkForBasicCardIssuerNetwork;
using ::autofill::data_util::GetPaymentRequestData;
using ::payment_request_util::GetBillingAddressLabelFromAutofillProfile;

// Returns true if |card_number| is a supported card type and a valid credit
// card number and no other credit card with the same number exists.
// |error_message| can't be null and will be filled with the appropriate error
// message iff the return value is false.
bool IsValidCreditCardNumber(const base::string16& card_number,
                             payments::PaymentRequest* payment_request,
                             const autofill::CreditCard* credit_card_to_edit,
                             base::string16* error_message) {
  std::set<std::string> supported_card_networks(
      payment_request->supported_card_networks().begin(),
      payment_request->supported_card_networks().end());
  return ::autofill::IsValidCreditCardNumberForBasicCardNetworks(
      card_number, supported_card_networks, error_message);
  // TODO(crbug.com/725604): The UI should offer to load / update the existing
  // credit card info if another local credit card has already been created with
  // this number. (Does not apply to server cards, which can be accessed only in
  // tokenized form through Google Pay.)
}

}  // namespace

@interface CreditCardEditMediator ()

// The PaymentRequest object owning an instance of payments::WebPaymentRequest
// as provided by the page invoking the Payment Request API. This is a weak
// pointer and should outlive this class.
@property(nonatomic, assign) payments::PaymentRequest* paymentRequest;

// The credit card to be edited, if any. This pointer is not owned by this class
// and should outlive it.
@property(nonatomic, assign) autofill::CreditCard* creditCard;

// The map of autofill types to the cached editor fields. Helps reuse the editor
// fields and therefore maintain their existing values when the billing address
// changes and the fields get updated.
@property(nonatomic, strong)
    NSMutableDictionary<NSNumber*, EditorField*>* fieldsMap;

// The reference to the expiration date field that combines the values for
// autofill::CREDIT_CARD_EXP_MONTH and autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR
// field types. This is nil when editing a server card.
@property(nonatomic, strong) EditorField* creditCardExpDateField;

@end

@implementation CreditCardEditMediator

@synthesize state = _state;
@synthesize consumer = _consumer;
@synthesize billingProfile = _billingProfile;
@synthesize paymentRequest = _paymentRequest;
@synthesize creditCard = _creditCard;
@synthesize fieldsMap = _fieldsMap;
@synthesize creditCardExpDateField = _creditCardExpDateField;

- (instancetype)initWithPaymentRequest:(payments::PaymentRequest*)paymentRequest
                            creditCard:(autofill::CreditCard*)creditCard {
  self = [super init];
  if (self) {
    _paymentRequest = paymentRequest;
    _creditCard = creditCard;
    _state = _creditCard ? EditViewControllerStateEdit
                         : EditViewControllerStateCreate;
    _billingProfile =
        _creditCard && !_creditCard->billing_address_id().empty()
            ? autofill::PersonalDataManager::GetProfileFromProfilesByGUID(
                  _creditCard->billing_address_id(),
                  _paymentRequest->billing_profiles())
            : nil;
    _fieldsMap = [[NSMutableDictionary alloc] init];
  }
  return self;
}

#pragma mark - Setters

- (void)setConsumer:(id<PaymentRequestEditConsumer>)consumer {
  _consumer = consumer;

  [self.consumer setEditorFields:[self createEditorFields]];
  if (self.creditCardExpDateField)
    [self loadMonthsAndYears];
}

- (void)setBillingProfile:(autofill::AutofillProfile*)billingProfile {
  _billingProfile = billingProfile;
  [self.consumer setEditorFields:[self createEditorFields]];
  if (self.creditCardExpDateField)
    [self loadMonthsAndYears];
}

#pragma mark - PaymentRequestEditViewControllerDataSource

- (NSString*)title {
  if (!self.creditCard)
    return l10n_util::GetNSString(IDS_PAYMENTS_ADD_CARD_LABEL);

  const payments::CreditCardCompletionStatus status =
      payments::GetCompletionStatusForCard(
          *self.creditCard, self.paymentRequest->GetApplicationLocale(),
          self.paymentRequest->billing_profiles());
  return base::SysUTF16ToNSString(payments::GetEditDialogTitleForCard(status));
}

- (CollectionViewItem*)headerItem {
  if (_creditCard && !autofill::IsCreditCardLocal(*_creditCard)) {
    // Return an item that identifies the server card being edited.
    PaymentMethodItem* cardSummaryItem = [[PaymentMethodItem alloc] init];
    cardSummaryItem.methodID =
        base::SysUTF16ToNSString(_creditCard->NetworkAndLastFourDigits());
    cardSummaryItem.methodDetail = base::SysUTF16ToNSString(
        _creditCard->GetRawInfo(autofill::CREDIT_CARD_NAME_FULL));
    const int issuerNetworkIconID =
        autofill::data_util::GetPaymentRequestData(_creditCard->network())
            .icon_resource_id;
    cardSummaryItem.methodTypeIcon = NativeImage(issuerNetworkIconID);
    return cardSummaryItem;
  }

  // Otherwise, return an item that displays a list of payment method type icons
  // for the accepted payment methods.
  NSMutableArray* issuerNetworkIcons = [NSMutableArray array];
  for (const auto& supportedNetwork :
       _paymentRequest->supported_card_networks()) {
    const std::string issuerNetwork =
        GetIssuerNetworkForBasicCardIssuerNetwork(supportedNetwork);
    const autofill::data_util::PaymentRequestData& cardData =
        GetPaymentRequestData(issuerNetwork);
    UIImage* issuerNetworkIcon = NativeImage(cardData.icon_resource_id);
    issuerNetworkIcon.accessibilityLabel =
        l10n_util::GetNSString(cardData.a11y_label_resource_id);
    [issuerNetworkIcons addObject:issuerNetworkIcon];
  }

  AcceptedPaymentMethodsItem* acceptedMethodsItem =
      [[AcceptedPaymentMethodsItem alloc] init];
  acceptedMethodsItem.message =
      base::SysUTF16ToNSString(payments::GetAcceptedCardTypesText(
          _paymentRequest->supported_card_types_set()));
  acceptedMethodsItem.methodTypeIcons = issuerNetworkIcons;
  return acceptedMethodsItem;
}

- (BOOL)shouldHideBackgroundForHeaderItem {
  // YES if the header item displays the accepted payment method type icons.
  return !_creditCard || autofill::IsCreditCardLocal(*_creditCard);
}

- (BOOL)shouldFormatValueForAutofillUIType:(AutofillUIType)type {
  return (type == AutofillUITypeCreditCardNumber);
}

- (NSString*)formatValue:(NSString*)value autofillUIType:(AutofillUIType)type {
  if (type == AutofillUITypeCreditCardNumber) {
    return base::SysUTF16ToNSString(
        payments::data_util::FormatCardNumberForDisplay(
            base::SysNSStringToUTF16(value)));
  }
  return nil;
}

- (UIImage*)iconIdentifyingEditorField:(EditorField*)field {
  // Early return if the field is not the credit card number field.
  if (field.autofillUIType != AutofillUITypeCreditCardNumber)
    return nil;

  const char* issuerNetwork = autofill::CreditCard::GetCardNetwork(
      base::SysNSStringToUTF16(field.value));
  // This should not happen in Payment Request.
  if (issuerNetwork == autofill::kGenericCard)
    return nil;

  // Return the card issuer network icon.
  int resourceID = autofill::data_util::GetPaymentRequestData(issuerNetwork)
                       .icon_resource_id;
  return NativeImage(resourceID);
}

#pragma mark - PaymentRequestEditViewControllerValidator

- (NSString*)paymentRequestEditViewController:
                 (PaymentRequestEditViewController*)controller
                                validateField:(EditorField*)field {
  if (field.value.length) {
    base::string16 errorMessage;
    base::string16 valueString = base::SysNSStringToUTF16(field.value);
    if (field.autofillUIType == AutofillUITypeCreditCardNumber) {
      ::IsValidCreditCardNumber(valueString, _paymentRequest, _creditCard,
                                &errorMessage);
    } else if (field.autofillUIType == AutofillUITypeCreditCardExpDate) {
      NSArray<NSString*>* fieldComponents =
          [field.value componentsSeparatedByString:@" / "];
      int expMonth = [fieldComponents[0] intValue];
      int expYear = [fieldComponents[1] intValue];
      if (!autofill::IsValidCreditCardExpirationDate(
              expYear, expMonth, autofill::AutofillClock::Now())) {
        errorMessage = l10n_util::GetStringUTF16(
            IDS_PAYMENTS_VALIDATION_INVALID_CREDIT_CARD_EXPIRED);
      }
    } else if (field.autofillUIType != AutofillUITypeCreditCardBillingAddress &&
               field.autofillUIType != AutofillUITypeCreditCardSaveToChrome) {
      autofill::IsValidForType(
          valueString, ::AutofillTypeFromAutofillUIType(field.autofillUIType),
          &errorMessage);
    }
    return !errorMessage.empty() ? base::SysUTF16ToNSString(errorMessage) : nil;
  } else if (field.isRequired) {
    return l10n_util::GetNSString(
        IDS_PREF_EDIT_DIALOG_FIELD_REQUIRED_VALIDATION_MESSAGE);
  }
  return nil;
}

#pragma mark - Helper methods

// Queries the month and year numbers and informs the consumer.
- (void)loadMonthsAndYears {
  NSMutableArray<NSString*>* months = [[NSMutableArray alloc] init];

  for (int month = 1; month <= 12; month++) {
    NSString* monthString = [NSString stringWithFormat:@"%02d", month];
    [months addObject:monthString];
  }

  NSMutableArray<NSString*>* years = [[NSMutableArray alloc] init];

  NSDateComponents* dateComponents =
      [[NSCalendar currentCalendar] components:NSCalendarUnitYear
                                      fromDate:[NSDate date]];

  int currentYear = [dateComponents year];
  int cardExpYear = _creditCard ? _creditCard->expiration_year() : currentYear;
  bool foundCardExpirationYear = false;
  for (int year = currentYear; year < currentYear + 10; year++) {
    if (year == cardExpYear)
      foundCardExpirationYear = true;
    [years addObject:[NSString stringWithFormat:@"%04d", year]];
  }

  // Ensure that the expiration year on a saved card is always available as one
  // of the options to select from. This is useful in case of an expired card.
  if (!foundCardExpirationYear) {
    [years insertObject:[NSString stringWithFormat:@"%04d", cardExpYear]
                atIndex:0];
  }

  // Notify the view controller asynchronously to allow for the view to update.
  __weak CreditCardEditMediator* weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    [weakSelf.consumer setOptions:@[ months, years ]
                   forEditorField:weakSelf.creditCardExpDateField];
  });
}

- (NSArray<EditorField*>*)createEditorFields {
  NSMutableArray<EditorField*>* fields = [[NSMutableArray alloc] init];

  // Billing address field.
  NSNumber* fieldKey =
      [NSNumber numberWithInt:AutofillUITypeCreditCardBillingAddress];
  EditorField* billingAddressField = self.fieldsMap[fieldKey];
  if (!billingAddressField) {
    billingAddressField = [[EditorField alloc]
        initWithAutofillUIType:AutofillUITypeCreditCardBillingAddress
                     fieldType:EditorFieldTypeSelector
                         label:l10n_util::GetNSString(
                                   IDS_PAYMENTS_BILLING_ADDRESS)
                         value:nil
                      required:YES];
    [self.fieldsMap setObject:billingAddressField forKey:fieldKey];
  }
  billingAddressField.value =
      self.billingProfile ? base::SysUTF8ToNSString(self.billingProfile->guid())
                          : nil;
  billingAddressField.displayValue =
      self.billingProfile
          ? GetBillingAddressLabelFromAutofillProfile(*self.billingProfile)
          : nil;

  // Early return if editing a server card. For server cards, only the billing
  // address can be edited.
  if (_creditCard && !autofill::IsCreditCardLocal(*_creditCard))
    return @[ billingAddressField ];

  // Credit Card number field.
  NSString* creditCardNumber =
      _creditCard ? base::SysUTF16ToNSString(
                        payments::data_util::FormatCardNumberForDisplay(
                            _creditCard->number()))
                  : nil;
  fieldKey = [NSNumber numberWithInt:AutofillUITypeCreditCardNumber];
  EditorField* creditCardNumberField = self.fieldsMap[fieldKey];
  if (!creditCardNumberField) {
    creditCardNumberField = [[EditorField alloc]
        initWithAutofillUIType:AutofillUITypeCreditCardNumber
                     fieldType:EditorFieldTypeTextField
                         label:l10n_util::GetNSString(IDS_PAYMENTS_CARD_NUMBER)
                         value:creditCardNumber
                      required:YES];
    creditCardNumberField.keyboardType = UIKeyboardTypeNumberPad;
    [self.fieldsMap setObject:creditCardNumberField forKey:fieldKey];
  }
  [fields addObject:creditCardNumberField];

  // Card holder name field.
  NSString* creditCardName =
      _creditCard ? autofill::GetCreditCardName(
                        *_creditCard, _paymentRequest->GetApplicationLocale())
                  : nil;
  fieldKey = [NSNumber numberWithInt:AutofillUITypeCreditCardHolderFullName];
  EditorField* creditCardNameField = self.fieldsMap[fieldKey];
  if (!creditCardNameField) {
    creditCardNameField = [[EditorField alloc]
        initWithAutofillUIType:AutofillUITypeCreditCardHolderFullName
                     fieldType:EditorFieldTypeTextField
                         label:l10n_util::GetNSString(IDS_PAYMENTS_NAME_ON_CARD)
                         value:creditCardName
                      required:YES];
    [self.fieldsMap setObject:creditCardNameField forKey:fieldKey];
  }
  [fields addObject:creditCardNameField];

  NSDateComponents* dateComponents = [[NSCalendar currentCalendar]
      components:NSCalendarUnitMonth | NSCalendarUnitYear
        fromDate:[NSDate date]];

  // Expiration date field.
  int currentMonth = [dateComponents month];
  NSString* expMonth =
      _creditCard
          ? [NSString stringWithFormat:@"%02d", _creditCard->expiration_month()]
          : [NSString stringWithFormat:@"%02d", currentMonth];
  int currentYear = [dateComponents year];
  NSString* expYear =
      _creditCard
          ? [NSString stringWithFormat:@"%04d", _creditCard->expiration_year()]
          : [NSString stringWithFormat:@"%04d", currentYear];
  NSString* expDate = [NSString stringWithFormat:@"%@ / %@", expMonth, expYear];
  fieldKey = [NSNumber numberWithInt:AutofillUITypeCreditCardExpDate];
  EditorField* expirationDateField = self.fieldsMap[fieldKey];
  if (!expirationDateField) {
    expirationDateField = [[EditorField alloc]
        initWithAutofillUIType:AutofillUITypeCreditCardExpDate
                     fieldType:EditorFieldTypeTextField
                         label:l10n_util::GetNSString(IDS_PAYMENTS_EXP_DATE)
                         value:expDate
                      required:YES];
    [self.fieldsMap setObject:expirationDateField forKey:fieldKey];
  }
  self.creditCardExpDateField = expirationDateField;
  [fields addObject:expirationDateField];

  // The billing address field appears after the expiration year field.
  [fields addObject:billingAddressField];

  // Save credit card field.
  fieldKey = [NSNumber numberWithInt:AutofillUITypeCreditCardSaveToChrome];
  EditorField* saveToChromeField = self.fieldsMap[fieldKey];
  if (!saveToChromeField) {
    saveToChromeField = [[EditorField alloc]
        initWithAutofillUIType:AutofillUITypeCreditCardSaveToChrome
                     fieldType:EditorFieldTypeSwitch
                         label:l10n_util::GetNSString(
                                   IDS_PAYMENTS_SAVE_CARD_TO_DEVICE_CHECKBOX)
                         value:@"YES"
                      required:YES];
    [self.fieldsMap setObject:saveToChromeField forKey:fieldKey];
  }
  [fields addObject:saveToChromeField];

  return fields;
}

@end
