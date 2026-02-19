// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_SCAN_SAVE_AND_FILL_COORDINATOR_PAYMENTS_SCAN_SAVE_AND_FILL_OFFER_BOTTOM_SHEET_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_SCAN_SAVE_AND_FILL_COORDINATOR_PAYMENTS_SCAN_SAVE_AND_FILL_OFFER_BOTTOM_SHEET_MEDIATOR_H_

#import <Foundation/Foundation.h>

namespace autofill {
struct FormActivityParams;
}  // namespace autofill

class WebStateList;

@protocol PaymentsScanSaveAndFillOfferBottomSheetConsumer;

@interface PaymentsScanSaveAndFillOfferBottomSheetMediator : NSObject
// Designated initializer. `webStateList` must not be nil.
- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                              params:(autofill::FormActivityParams)params
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, weak) id<PaymentsScanSaveAndFillOfferBottomSheetConsumer>
    consumer;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_SCAN_SAVE_AND_FILL_COORDINATOR_PAYMENTS_SCAN_SAVE_AND_FILL_OFFER_BOTTOM_SHEET_MEDIATOR_H_
