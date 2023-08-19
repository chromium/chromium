// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_BOTTOM_SHEET_PAYMENTS_SUGGESTION_BOTTOM_SHEET_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_BOTTOM_SHEET_PAYMENTS_SUGGESTION_BOTTOM_SHEET_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/ui/autofill/bottom_sheet/payments_suggestion_bottom_sheet_handler.h"

namespace autofill {
struct FormActivityParams;
}  // namespace autofill

@protocol ApplicationCommands;

// This coordinator is responsible for creating the bottom sheet's mediator and
// view controller.
@interface PaymentsSuggestionBottomSheetCoordinator
    : ChromeCoordinator <PaymentsSuggestionBottomSheetHandler>

// `viewController` is the VC used to present the bottom sheet.
// `params` comes from the form (in bottom_sheet.ts) and contains
// the information required to query payments suggestions.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                    params:(const autofill::FormActivityParams&)
                                               params;

// Handler for Application Commands.
@property(nonatomic, weak) id<ApplicationCommands> applicationCommandsHandler;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_BOTTOM_SHEET_PAYMENTS_SUGGESTION_BOTTOM_SHEET_COORDINATOR_H_
