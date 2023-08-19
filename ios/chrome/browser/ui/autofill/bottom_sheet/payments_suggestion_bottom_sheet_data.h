// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_BOTTOM_SHEET_PAYMENTS_SUGGESTION_BOTTOM_SHEET_DATA_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_BOTTOM_SHEET_PAYMENTS_SUGGESTION_BOTTOM_SHEET_DATA_H_

#import <UIKit/UIKit.h>

namespace autofill {
class CreditCard;
}  // namespace autofill

// Data source for each individual credit card suggestion.
@protocol PaymentsSuggestionBottomSheetData

// Returns the credit card's name and last four digits of the card.
// Will return the credit card's nickname if it has one.
- (NSString*)cardNameAndLastFourDigits;

// Returns the credit card's expiration date or type.
- (NSString*)cardDetails;

// Returns the credit card's backend identifier.
- (NSString*)backendIdentifier;

// Returns an accessible card name.
- (NSString*)accessibleCardName;

// Returns the icon associated with the "creditCard" above.
- (UIImage*)icon;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_BOTTOM_SHEET_PAYMENTS_SUGGESTION_BOTTOM_SHEET_DATA_H_
