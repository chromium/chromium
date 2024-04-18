// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_CARD_LIST_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_CARD_LIST_DELEGATE_H_

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_constants.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_credit_card.h"

@class CrURL;

// Delegate for actions in manual fallback's cards list.
@protocol CardListDelegate

// Opens feature to add a credit card.
- (void)openAddCreditCard;

// Opens cards settings.
- (void)openCardSettings;

// Opens credit card unlock, through CVC, prompt.
- (void)requestFullCreditCard:(ManualFillCreditCard*)card
                    fieldType:(manual_fill::PaymentFieldType)fieldType;

// Opens URL in a bottom sheet view with the given title.
- (void)openURL:(CrURL*)url withTitle:(NSString*)title;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_CARD_LIST_DELEGATE_H_
