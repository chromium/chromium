// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_CARD_LIST_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_CARD_LIST_DELEGATE_H_

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_credit_card.h"

// Delegate for actions in manual fallback's cards list.
@protocol CardListDelegate

// Open feature to add a credit card.
- (void)openAddCreditCard;

// Open cards settings.
- (void)openCardSettings;

// Open credit card unlock, through CVC, prompt.
- (void)requestFullCreditCard:(ManualFillCreditCard*)card;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_CARD_LIST_DELEGATE_H_
