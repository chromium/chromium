// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/credit_card/credit_card_data.h"

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

@implementation CreditCardData

- (instancetype)initWithCreditCard:(const autofill::CreditCard&)creditCard
                              icon:(UIImage*)icon {
  if ((self = [super init])) {
    _cardNameAndLastFourDigits =
        base::SysUTF16ToNSString(creditCard.CardNameAndLastFourDigits());
    _cardDetails = base::SysUTF16ToNSString(
        (creditCard.record_type() ==
         autofill::CreditCard::RecordType::kVirtualCard)
            ? l10n_util::GetStringUTF16(
                  IDS_AUTOFILL_VIRTUAL_CARD_SUGGESTION_OPTION_VALUE)
            : creditCard.AbbreviatedExpirationDateForDisplay(
                  /* with_prefix=*/false));
    _accessibleCardName = [self accessibleCardName:creditCard];
    _backendIdentifier = base::SysUTF8ToNSString(creditCard.guid());
    _recordType = creditCard.record_type();

    if (icon.size.width > 0.0 && icon.size.width < 40.0 && icon.scale > 1.0) {
      // If the icon is smaller than desired, but is scaled, reduce the scale
      // (to a minimum of 1.0) in order to attempt to achieve the desired size.
      _icon = [UIImage
          imageWithCGImage:[icon CGImage]
                     scale:MAX((icon.scale * icon.size.width / 40.0), 1.0)
               orientation:(icon.imageOrientation)];
    } else {
      _icon = icon;
    }
  }
  return self;
}

#pragma mark - Private

- (NSString*)accessibleCardName:(const autofill::CreditCard&)creditCard {
  // Get the card name. Prepend the card type if the card name doesn't already
  // start with the card type.
  NSString* cardType = base::SysUTF16ToNSString(
      creditCard.GetRawInfo(autofill::CREDIT_CARD_TYPE));
  NSString* cardAccessibleName =
      base::SysUTF16ToNSString(creditCard.CardNameForAutofillDisplay());
  if (![cardAccessibleName hasPrefix:cardType]) {
    // If the card name doesn't already start with the card type, add the card
    // type at the beginning of the card name.
    cardAccessibleName =
        [@[ cardType, cardAccessibleName ] componentsJoinedByString:@" "];
  }

  // Split the last 4 digits, so that they are pronounced separately. For
  // example, "1215" will become "1 2 1 5" and will read "one two one five"
  // instead of "one thousand two hundred and fifteen".
  NSString* cardLastDigits =
      base::SysUTF16ToNSString(creditCard.LastFourDigits());
  NSMutableArray* digits = [[NSMutableArray alloc] init];
  if (cardLastDigits.length > 0) {
    for (NSUInteger i = 0; i < cardLastDigits.length; i++) {
      [digits addObject:[cardLastDigits substringWithRange:NSMakeRange(i, 1)]];
    }
    cardLastDigits = [digits componentsJoinedByString:@" "];
  }

  // Add mention that the credit card ends with the last 4 digits.
  cardAccessibleName = base::SysUTF16ToNSString(
      l10n_util::GetStringFUTF16(IDS_IOS_PAYMENT_BOTTOM_SHEET_CARD_DESCRIPTION,
                                 base::SysNSStringToUTF16(cardAccessibleName),
                                 base::SysNSStringToUTF16(cardLastDigits)));

  // Either prepend that the card is a virtual card OR append the expiration
  // date.
  if (creditCard.record_type() ==
      autofill::CreditCard::RecordType::kVirtualCard) {
    cardAccessibleName = [@[ self.cardDetails, cardAccessibleName ]
        componentsJoinedByString:@" "];
  } else {
    cardAccessibleName = base::SysUTF16ToNSString(l10n_util::GetStringFUTF16(
        IDS_AUTOFILL_CREDIT_CARD_TWO_LINE_LABEL_FROM_NAME,
        base::SysNSStringToUTF16(cardAccessibleName),
        base::SysNSStringToUTF16(self.cardDetails)));
  }

  return cardAccessibleName;
}

@end
