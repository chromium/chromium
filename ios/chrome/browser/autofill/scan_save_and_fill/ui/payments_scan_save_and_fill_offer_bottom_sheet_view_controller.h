// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_SCAN_SAVE_AND_FILL_UI_PAYMENTS_SCAN_SAVE_AND_FILL_OFFER_BOTTOM_SHEET_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_SCAN_SAVE_AND_FILL_UI_PAYMENTS_SCAN_SAVE_AND_FILL_OFFER_BOTTOM_SHEET_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/autofill/scan_save_and_fill/ui/payments_scan_save_and_fill_offer_bottom_sheet_consumer.h"
#import "ios/chrome/browser/shared/ui/bottom_sheet/table_view_bottom_sheet_view_controller.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"

@protocol PaymentsScanSaveAndFillOfferBottomSheetDelegate;

@interface PaymentsScanSaveAndFillOfferBottomSheetViewController
    : TableViewBottomSheetViewController <
          PaymentsScanSaveAndFillOfferBottomSheetConsumer>

@property(nonatomic, weak) id<PaymentsScanSaveAndFillOfferBottomSheetDelegate>
    delegate;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_SCAN_SAVE_AND_FILL_UI_PAYMENTS_SCAN_SAVE_AND_FILL_OFFER_BOTTOM_SHEET_VIEW_CONTROLLER_H_
