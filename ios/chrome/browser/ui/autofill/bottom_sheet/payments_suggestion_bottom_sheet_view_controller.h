// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_BOTTOM_SHEET_PAYMENTS_SUGGESTION_BOTTOM_SHEET_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_BOTTOM_SHEET_PAYMENTS_SUGGESTION_BOTTOM_SHEET_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/autofill/bottom_sheet/payments_suggestion_bottom_sheet_consumer.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"

// Payments Bottom Sheet UI, which includes a table to display payments
// suggestions, a button to use a suggestion and a button to revert to
// using the keyboard to enter the payment information.
@interface PaymentsSuggestionBottomSheetViewController
    : ConfirmationAlertViewController <PaymentsSuggestionBottomSheetConsumer>

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_BOTTOM_SHEET_PAYMENTS_SUGGESTION_BOTTOM_SHEET_VIEW_CONTROLLER_H_
