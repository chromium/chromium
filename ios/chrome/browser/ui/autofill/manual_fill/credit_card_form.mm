// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/credit_card_form.h"

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#import "components/autofill/ios/browser/credit_card_util.h"
#include "ios/chrome/browser/application_context.h"
#include "url/gurl.h"

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
  constexpr base::char16 separator[] = {0x2060, 0x0020, 0};
  const base::string16 digits = creditCard.LastFourDigits();
  NSString* obfuscatedNumber = base::SysUTF16ToNSString(
      autofill::kMidlineEllipsis + base::string16(separator) +
      autofill::kMidlineEllipsis + base::string16(separator) +
      autofill::kMidlineEllipsis + base::string16(separator) + digits);

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
