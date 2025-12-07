// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/credit_card_scanner/credit_card_scanner_string_util.h"

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_model/payments/credit_card.h"

NSDateComponents* ExtractExpirationDateFromText(NSString* string) {
  NSString* text = [[NSString alloc] initWithString:string];
  // Avoid processing too much text, in the unlikely case that the vision
  // library glitches and sends us a massive string.
  if ([text length] > 50) {
    text = [string substringToIndex:50];
  }

  // Extract dates from longer strings, e.g. "Valid thru 01/30"
  //
  // TODO(crbug.com/442869727): Although no examples have been seen so far, it
  // may be possible for an expiration date to have a single-digit month or a
  // four-digit year. Expand the regex to handle these cases.
  NSString* pattern = @"(0[1-9]|1[0-2])\\/\\d{2}";
  NSError* error = nil;
  NSRegularExpression* regex =
      [NSRegularExpression regularExpressionWithPattern:pattern
                                                options:0
                                                  error:&error];
  if (error) {
    return nil;
  }

  NSArray<NSTextCheckingResult*>* matches =
      [regex matchesInString:text
                     options:0
                       range:NSMakeRange(0, [text length])];

  if ([matches count] < 1) {
    return nil;
  }

  // Currently we support only the first match, with the assumption that it will
  // be rare to extract a single string with two dates in it.
  NSString* dateString = [text substringWithRange:[matches[0] range]];

  NSDateFormatter* formatter = [[NSDateFormatter alloc] init];
  [formatter setDateFormat:@"MM/yy"];
  NSDate* date = [formatter dateFromString:dateString];

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
  // Avoid processing too much text, in the unlikely case that the vision
  // library glitches and sends us a massive string.
  if ([text length] > 50) {
    text = [string substringToIndex:50];
  }

  // Strip whitespaces and symbols.
  NSArray* ignoreSymbols = @[ @" ", @"/", @"-", @".", @":", @"\\" ];
  for (NSString* symbol in ignoreSymbols) {
    text = [text stringByReplacingOccurrencesOfString:symbol withString:@""];
  }

  // Matches strings which have 13-19 characters between the start(^) and the
  // end($) of the line; this covers all major credit card number lengths.
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

  // Finally, validate that the extracted number is a valid credit card number,
  // i.e. that it passes a luhn checksum.
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
