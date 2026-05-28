// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_UI_MANUAL_FILL_CARD_CELL_TESTING_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_UI_MANUAL_FILL_CARD_CELL_TESTING_H_

#import "ios/chrome/browser/autofill/manual_fill/ui/manual_fill_card_cell.h"

// Testing category exposing private properties of ManualFillCardItem for tests.
@interface ManualFillCardItem (Testing)

// The 0-based index at which the payment method is in the list of payment
// methods to show.
@property(nonatomic, assign, readonly) NSInteger cellIndex;

// The part of the cell's accessibility label that is used to indicate the
// 1-based index at which the payment method represented by this item is
// positioned in the list of payment methods to show.
@property(nonatomic, copy, readonly) NSString* cellIndexAccessibilityLabel;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_UI_MANUAL_FILL_CARD_CELL_TESTING_H_
