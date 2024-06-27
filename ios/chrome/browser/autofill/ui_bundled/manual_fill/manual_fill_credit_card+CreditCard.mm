// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/autofill/core/browser/autofill_data_util.h"
#import "components/autofill/core/browser/data_model/credit_card.h"
#import "components/autofill/core/browser/validation.h"
#import "components/autofill/core/common/credit_card_number_validation.h"
#import "components/autofill/ios/browser/credit_card_util.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_credit_card+CreditCard.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "url/gurl.h"

@implementation ManualFillCreditCard (CreditCardForm)

- (instancetype)initWithCreditCard:(const autofill::CreditCard&)creditCard
                              icon:(UIImage*)icon {
  NSString* GUID =
      base::SysUTF16ToNSString(base::ASCIIToUTF16(creditCard.guid()));
  NSString* network = base::SysUTF16ToNSString(creditCard.NetworkForDisplay());
  NSString* bankName =
      base::SysUTF16ToNSString(base::ASCIIToUTF16(creditCard.bank_name()));
  NSString* cardHolder = autofill::GetCreditCardName(
      creditCard, GetApplicationContext()->GetApplicationLocale());
  NSString* number = nil;
  if (creditCard.record_type() !=
      autofill::CreditCard::RecordType::kMaskedServerCard) {
    number = base::SysUTF16ToNSString(autofill::StripCardNumberSeparators(
        creditCard.GetRawInfo(autofill::CREDIT_CARD_NUMBER)));
  }

  BOOL canFillDirectly =
      (creditCard.record_type() !=
       autofill::CreditCard::RecordType::kMaskedServerCard) &&
      (creditCard.record_type() !=
       autofill::CreditCard::RecordType::kVirtualCard);

  // Unicode characters used in card number:
  //  - 0x0020 - Space.
  //  - 0x2060 - WORD-JOINER (makes string undivisible).
  constexpr char16_t separator[] = {0x2060, 0x0020, 0};
  const std::u16string digits = creditCard.LastFourDigits();
  NSString* obfuscatedNumber =
      base::SysUTF16ToNSString(autofill::CreditCard::GetMidlineEllipsisDots(4) +
                               std::u16string(separator) +
                               autofill::CreditCard::GetMidlineEllipsisDots(4) +
                               std::u16string(separator) +
                               autofill::CreditCard::GetMidlineEllipsisDots(4) +
                               std::u16string(separator) + digits);

  NSString* networkAndLastFourDigits =
      base::SysUTF16ToNSString(creditCard.NetworkAndLastFourDigits());

  // Use 2 digits year.
  NSString* expirationYear =
      [NSString stringWithFormat:@"%02d", creditCard.expiration_year() % 100];
  NSString* expirationMonth =
      [NSString stringWithFormat:@"%02d", creditCard.expiration_month()];

  NSString* CVC = nil;
  if (creditCard.record_type() ==
      autofill::CreditCard::RecordType::kVirtualCard) {
    if (creditCard.cvc().empty()) {
      // For virtual cards, if the CVC() value is empty, it means no
      // verification has been done and the `creditCard` object contains only
      // the obfuscated card information.
      CVC =
          base::SysUTF16ToNSString(autofill::CreditCard::GetMidlineEllipsisDots(
              autofill::GetCvcLengthForCardNetwork(creditCard.network())));
    } else {
      // If the CVC() value is non-empty, it means the a verification step has
      // been done and the `creditCard` object contains the full card
      // information.
      CVC = base::SysUTF16ToNSString(creditCard.cvc());
    }
  }

  return [self initWithGUID:GUID
                       network:network
                          icon:icon
                      bankName:bankName
                    cardHolder:cardHolder
                        number:number
              obfuscatedNumber:obfuscatedNumber
      networkAndLastFourDigits:networkAndLastFourDigits
                expirationYear:expirationYear
               expirationMonth:expirationMonth
                           CVC:CVC
                    recordType:creditCard.record_type()
               canFillDirectly:canFillDirectly];
}

@end
