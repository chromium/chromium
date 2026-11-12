// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_UI_MANUAL_FILL_CARD_CELL_MUTATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_UI_MANUAL_FILL_CARD_CELL_MUTATOR_H_

#import <Foundation/Foundation.h>

@class ManualFillCreditCard;

// Credit card field chips available in `ManualFillCardCell`.
enum class ManualFillCardFieldType {
  kCardNumber,
  kCardholderName,
  kExpirationMonth,
  kExpirationYear,
  kCVC,
};

// Mutator for `ManualFillCardCell` user actions.
@protocol ManualFillCardCellMutator <NSObject>

// Called when the user taps a field chip of `fieldType` for `card`.
- (void)didTapChipForCard:(ManualFillCreditCard*)card
                fieldType:(ManualFillCardFieldType)fieldType;

// Called when the user taps the "Autofill Form" button for `card` at
// `cellIndex`.
- (void)didTapAutofillFormButtonForCard:(ManualFillCreditCard*)card
                              cellIndex:(NSInteger)cellIndex;

// Called when the user taps the "Learn more" link in the virtual card
// instruction text view.
- (void)didTapVirtualCardLearnMore;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_UI_MANUAL_FILL_CARD_CELL_MUTATOR_H_
