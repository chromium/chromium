// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CREDIT_CARD_SCANNER_CREDIT_CARD_SCANNER_STRING_UTIL_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CREDIT_CARD_SCANNER_CREDIT_CARD_SCANNER_STRING_UTIL_H_

#import <Foundation/Foundation.h>

// Extracts credit card expiration month and year from `string`.
NSDateComponents* ExtractExpirationDateFromText(NSString* string);

// Extracts credit card number from `string`.
NSString* ExtractCreditCardNumber(NSString* string);

// Substitutes commonly misrecognized characters, for example: 'S' -> '5' or
// 'l' -> '1'
NSString* SubstituteSimilarCharactersInRecognizedText(NSString* recognizedText);

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CREDIT_CARD_SCANNER_CREDIT_CARD_SCANNER_STRING_UTIL_H_
