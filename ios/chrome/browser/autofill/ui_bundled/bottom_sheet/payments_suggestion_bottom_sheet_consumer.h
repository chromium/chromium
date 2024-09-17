// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_PAYMENTS_SUGGESTION_BOTTOM_SHEET_CONSUMER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_PAYMENTS_SUGGESTION_BOTTOM_SHEET_CONSUMER_H_

#import <Foundation/Foundation.h>

@class CreditCardData;

// Suggestions consumer for the payments bottom sheet.
@protocol PaymentsSuggestionBottomSheetConsumer

// Sends the list of credit cards to be presented to the user on the bottom
// sheet and a BOOL to determine if the user sees the GPay logo as title of the
// bottom sheet.
- (void)setCreditCardData:(NSArray<CreditCardData*>*)creditCardData
        showGooglePayLogo:(BOOL)showGooglePayLogo;

// Request to dismiss the bottom sheet.
- (void)dismiss;

// Activate the primary button so its actions are now effective.
- (void)activatePrimaryButton;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_PAYMENTS_SUGGESTION_BOTTOM_SHEET_CONSUMER_H_
