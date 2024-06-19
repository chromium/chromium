// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_PAYMENTS_SUGGESTION_BOTTOM_SHEET_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_PAYMENTS_SUGGESTION_BOTTOM_SHEET_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/ui/bottom_sheet/table_view_bottom_sheet_view_controller.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/payments_suggestion_bottom_sheet_consumer.h"

@protocol PaymentsSuggestionBottomSheetDelegate;
@protocol PaymentsSuggestionBottomSheetHandler;

class GURL;

// Payments Bottom Sheet UI, which includes a table to display payments
// suggestions, a button to use a suggestion and a button to revert to
// using the keyboard to enter the payment information.
@interface PaymentsSuggestionBottomSheetViewController
    : TableViewBottomSheetViewController <PaymentsSuggestionBottomSheetConsumer>

// Initialize with the delegate used to open payments methods and the URL of the
// current page.
- (instancetype)initWithHandler:
                    (id<PaymentsSuggestionBottomSheetHandler>)handler
                            URL:(const GURL&)URL;

// The delegate for the bottom sheet view controller.
@property(nonatomic, strong) id<PaymentsSuggestionBottomSheetDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_PAYMENTS_SUGGESTION_BOTTOM_SHEET_VIEW_CONTROLLER_H_
