// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/credit_card_scanner/credit_card_scanner_string_util.h"

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_model/payments/credit_card.h"

namespace {

// Cached regex for extracting expiration dates.
// Matches M/YY, MM/YY, M/YYYY, and MM/YYYY formats.
NSRegularExpression* ExpirationDateRegex() {
  static NSRegularExpression* regex;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    regex =
        [NSRegularExpression regularExpressionWithPattern:
                                 @"\\b(0?[1-9]|1[0-2])\\/([0-9]{4}|[0-9]{2})\\b"
                                                  options:0
                                                    error:nil];
  });
  return regex;
}

// Matches strings which have 13-19 characters between the start(^) and the
// end($) of the line; this covers all major credit card number lengths.
NSRegularExpression* CardNumberRegex() {
  static NSRegularExpression* regex;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    regex = [NSRegularExpression
        regularExpressionWithPattern:@"^(\\w{13,19})$"
                             options:
                                 NSRegularExpressionAllowCommentsAndWhitespace
                               error:nil];
  });
  return regex;
}

// Cached regex for stripping symbols from credit card number text.
NSRegularExpression* SymbolsToStripRegex() {
  static NSRegularExpression* regex;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    regex = [NSRegularExpression regularExpressionWithPattern:@"[ /\\-\\.:\\\\]"
                                                      options:0
                                                        error:nil];
  });
  return regex;
}

}  // namespace

NSDateComponents* ExtractExpirationDateFromText(NSString* string) {
  // Avoid processing too much text, in the unlikely case that the vision
  // library glitches and sends us a massive string.
  if ([string length] > 50) {
    string = [string substringToIndex:50];
  }

  NSRegularExpression* regex = ExpirationDateRegex();

  NSArray<NSTextCheckingResult*>* matches =
      [regex matchesInString:string
                     options:0
                       range:NSMakeRange(0, [string length])];

  if ([matches count] < 1) {
    return nil;
  }

  // Currently we support only the first match, with the assumption that it will
  // be rare to extract a single string with two dates in it.
  NSTextCheckingResult* match = matches[0];

  // The regex pattern has two capture groups: month at index 1, and year at
  // index 2.
  if (match.numberOfRanges < 3) {
    return nil;
  }

  NSString* monthString = [string substringWithRange:[match rangeAtIndex:1]];
  NSString* yearString = [string substringWithRange:[match rangeAtIndex:2]];

  NSDateComponents* components = [[NSDateComponents alloc] init];
  components.month = [monthString integerValue];

  NSInteger year = [yearString integerValue];
  // If the parsed year is 4 digits (e.g., "2025"), convert it to a 2-digit year
  // ("25") to match the MM/YY display format.
  if (yearString.length == 4) {
    year %= 100;
  }
  components.year = year;

  return components;
}

NSString* ExtractCreditCardNumber(NSString* string) {
  // Avoid processing too much text, in the unlikely case that the vision
  // library glitches and sends us a massive string.
  if ([string length] > 50) {
    string = [string substringToIndex:50];
  }

  NSMutableString* mutableString = [string mutableCopy];
  [SymbolsToStripRegex()
      replaceMatchesInString:mutableString
                     options:0
                       range:NSMakeRange(0, mutableString.length)
                withTemplate:@""];

  NSRegularExpression* regex = CardNumberRegex();

  NSRange rangeOfText = NSMakeRange(0, [mutableString length]);
  NSTextCheckingResult* match = [regex firstMatchInString:mutableString
                                                  options:0
                                                    range:rangeOfText];
  if (!match) {
    return nil;
  }

  NSString* stringMatchingPattern =
      [mutableString substringWithRange:match.range];
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
  static NSDictionary<NSString*, NSString*>* substitutions;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    substitutions = @{
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
  });

  NSMutableString* result = [recognizedText.uppercaseString mutableCopy];
  for (NSString* letter in substitutions) {
    [result replaceOccurrencesOfString:letter
                            withString:substitutions[letter]
                               options:0
                                 range:NSMakeRange(0, result.length)];
  }
  return result;
}
