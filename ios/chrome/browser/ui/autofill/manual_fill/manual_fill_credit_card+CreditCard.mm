// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_credit_card+CreditCard.h"

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/autofill/core/browser/autofill_data_util.h"
#import "components/autofill/core/browser/data_model/credit_card.h"
#import "components/autofill/ios/browser/credit_card_util.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ManualFillCreditCard (CreditCardForm)

- (instancetype)initWithCreditCard:(const autofill::CreditCard&)creditCard {
  NSString* GUID =
      base::SysUTF16ToNSString(base::ASCIIToUTF16(creditCard.guid()));
  NSString* network = base::SysUTF16ToNSString(creditCard.NetworkForDisplay());
  NSString* bankName =
      base::SysUTF16ToNSString(base::ASCIIToUTF16(creditCard.bank_name()));
  NSString* cardHolder = autofill::GetCreditCardName(
      creditCard, GetApplicationContext()->GetApplicationLocale());
  NSString* number = nil;
  if (creditCard.record_type() != autofill::CreditCard::MASKED_SERVER_CARD) {
    number = base::SysUTF16ToNSString(autofill::CreditCard::StripSeparators(
        creditCard.GetRawInfo(autofill::CREDIT_CARD_NUMBER)));
  }
  const int issuerNetworkIconID =
      autofill::data_util::GetPaymentRequestData(creditCard.network())
          .icon_resource_id;

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

  // Use 2 digits year.
  NSString* expirationYear =
      [NSString stringWithFormat:@"%02d", creditCard.expiration_year() % 100];
  NSString* expirationMonth =
      [NSString stringWithFormat:@"%02d", creditCard.expiration_month()];

  return [self initWithGUID:GUID
                    network:network
        issuerNetworkIconID:issuerNetworkIconID
                   bankName:bankName
                 cardHolder:cardHolder
                     number:number
           obfuscatedNumber:obfuscatedNumber
             expirationYear:expirationYear
            expirationMonth:expirationMonth];
}

@end
