// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/credit_card_scanner/credit_card_scanner_string_util.h"

#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/data_model/credit_card.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios {

NSDateComponents* ExtractExpirationDateFromText(NSString* text) {
  NSDateFormatter* formatter = [[NSDateFormatter alloc] init];
  [formatter setDateFormat:@"MM/yy"];
  NSDate* date = [formatter dateFromString:text];

  if (date) {
    NSCalendar* gregorian = [[NSCalendar alloc]
        initWithCalendarIdentifier:NSCalendarIdentifierGregorian];
    return [gregorian components:NSCalendarUnitMonth | NSCalendarUnitYear
                        fromDate:date];
  }

  return nil;
}

NSString* ExtractCreditCardNumber(NSString* string) {
  NSString* text = [[NSString alloc] initWithString:string];
  // Strip whitespaces and symbols.
  NSArray* ignoreSymbols = @[ @" ", @"/", @"-", @".", @":", @"\\" ];
  for (NSString* symbol in ignoreSymbols) {
    text = [text stringByReplacingOccurrencesOfString:symbol withString:@""];
  }

  // Matches strings which have 13-19 characters between the start(^) and the
  // end($) of the line.
  NSString* pattern = @"^(\\w{13,19})$";

  NSError* error;
  NSRegularExpression* regex = [[NSRegularExpression alloc]
      initWithPattern:pattern
              options:NSRegularExpressionAllowCommentsAndWhitespace
                error:&error];

  NSRange rangeOfText = NSMakeRange(0, [text length]);
  NSTextCheckingResult* match = [regex firstMatchInString:text
                                                  options:0
                                                    range:rangeOfText];
  if (!match) {
    return nil;
  }

  NSString* stringMatchingPattern = [text substringWithRange:match.range];
  NSString* creditCardNumber =
      SubstituteSimilarCharactersInRecognizedText(stringMatchingPattern);

  autofill::CreditCard creditCard = autofill::CreditCard();
  creditCard.SetNumber(base::SysNSStringToUTF16(creditCardNumber));
  if (creditCard.HasValidCardNumber()) {
    return creditCardNumber;
  }

  return nil;
}

NSString* SubstituteSimilarCharactersInRecognizedText(
    NSString* recognizedText) {
  NSDictionary* misrecognisedAlphabets = @{
    @"B" : @"8",
    @"C" : @"0",
    @"D" : @"0",
    @"G" : @"9",
    @"I" : @"1",
    @"L" : @"1",
    @"O" : @"0",
    @"Q" : @"0",
    @"S" : @"5",
    @"T" : @"7",
    @"U" : @"0",
    @"Z" : @"7"
  };

  NSString* substitutedText =
      [[NSString alloc] initWithString:recognizedText].uppercaseString;
  for (NSString* alphabet in misrecognisedAlphabets) {
    NSString* digit = misrecognisedAlphabets[alphabet];
    substitutedText =
        [substitutedText stringByReplacingOccurrencesOfString:alphabet
                                                   withString:digit];
  }
  return substitutedText;
}

}  // namespace ios
