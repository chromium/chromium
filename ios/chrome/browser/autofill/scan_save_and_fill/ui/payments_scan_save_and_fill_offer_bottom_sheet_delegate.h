// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_SCAN_SAVE_AND_FILL_UI_PAYMENTS_SCAN_SAVE_AND_FILL_OFFER_BOTTOM_SHEET_DELEGATE_H_
#define IOS_CHROME_BROWSER_AUTOFILL_SCAN_SAVE_AND_FILL_UI_PAYMENTS_SCAN_SAVE_AND_FILL_OFFER_BOTTOM_SHEET_DELEGATE_H_

#import <Foundation/Foundation.h>

@protocol PaymentsScanSaveAndFillOfferBottomSheetDelegate

// Called when the view appeared.
- (void)paymentsBottomSheetViewDidAppear;

// Called when the view disappeared.
- (void)paymentsBottomSheetDidDisappear;

// Called when the user tapped on the scan card button.
- (void)didTapScanCardButton;

// Called when the user tapped on the cancel action button.
- (void)didTapOnCancelButton;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_SCAN_SAVE_AND_FILL_UI_PAYMENTS_SCAN_SAVE_AND_FILL_OFFER_BOTTOM_SHEET_DELEGATE_H_
