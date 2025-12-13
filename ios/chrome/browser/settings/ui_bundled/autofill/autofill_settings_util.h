// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_AUTOFILL_AUTOFILL_SETTINGS_UTIL_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_AUTOFILL_AUTOFILL_SETTINGS_UTIL_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/autofill/ui_bundled/autofill_credit_card_ui_type.h"

@class TableViewTextEditItem;

// Utility class for Autofill Settings UI.
@interface AutofillSettingsUtil : NSObject

// Updates the `cellAccessibilityLabel` of the given `item` to include:
// 1. The Field Name (e.g., "Card Number").
// 2. The Essential Placeholder (e.g., "MM/YY" or "Optional"), but only if the
//    field has text (since iOS hides the visual placeholder).
// 3. The `errorMessage` (e.g., "Invalid Card Number"), if `isValid` is NO.
+ (void)updateAccessibilityLabelForItem:(TableViewTextEditItem*)item
                           isInputValid:(BOOL)isValid
                           errorMessage:(NSString*)errorMessage;

// Returns the localized error message for the given credit card field type.
+ (NSString*)errorMessageForUIType:(AutofillCreditCardUIType)type;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_AUTOFILL_AUTOFILL_SETTINGS_UTIL_H_
