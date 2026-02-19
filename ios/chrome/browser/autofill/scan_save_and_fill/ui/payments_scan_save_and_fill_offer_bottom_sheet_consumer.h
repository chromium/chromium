// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_SCAN_SAVE_AND_FILL_UI_PAYMENTS_SCAN_SAVE_AND_FILL_OFFER_BOTTOM_SHEET_CONSUMER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_SCAN_SAVE_AND_FILL_UI_PAYMENTS_SCAN_SAVE_AND_FILL_OFFER_BOTTOM_SHEET_CONSUMER_H_

#import <Foundation/Foundation.h>

// Consumer for the payments scan save and fill offer bottom sheet.
@protocol PaymentsScanSaveAndFillOfferBottomSheetConsumer

// Request to dismiss the bottom sheet.
- (void)dismiss;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_SCAN_SAVE_AND_FILL_UI_PAYMENTS_SCAN_SAVE_AND_FILL_OFFER_BOTTOM_SHEET_CONSUMER_H_
