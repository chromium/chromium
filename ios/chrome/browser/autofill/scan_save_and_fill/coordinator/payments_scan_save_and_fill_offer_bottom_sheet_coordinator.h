// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_SCAN_SAVE_AND_FILL_COORDINATOR_PAYMENTS_SCAN_SAVE_AND_FILL_OFFER_BOTTOM_SHEET_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_SCAN_SAVE_AND_FILL_COORDINATOR_PAYMENTS_SCAN_SAVE_AND_FILL_OFFER_BOTTOM_SHEET_COORDINATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/autofill/scan_save_and_fill/ui/payments_scan_save_and_fill_offer_bottom_sheet_delegate.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

namespace autofill {
struct FormActivityParams;
}

// This coordinator is responsible for creating the bottom sheet's mediator and
// view controller. This is the coordinator that will trigger scan card bottom
// sheet to show when the user clicks on a credit card form but does not have
// any cards saved.
@interface PaymentsScanSaveAndFillOfferBottomSheetCoordinator
    : ChromeCoordinator <PaymentsScanSaveAndFillOfferBottomSheetDelegate>

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                                    params:(autofill::FormActivityParams)params;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_SCAN_SAVE_AND_FILL_COORDINATOR_PAYMENTS_SCAN_SAVE_AND_FILL_OFFER_BOTTOM_SHEET_COORDINATOR_H_
