// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PLUS_ADDRESSES_UI_PLUS_ADDRESS_BOTTOM_SHEET_CONSUMER_H_
#define IOS_CHROME_BROWSER_PLUS_ADDRESSES_UI_PLUS_ADDRESS_BOTTOM_SHEET_CONSUMER_H_

#import <Foundation/Foundation.h>

#import "components/plus_addresses/metrics/plus_address_metrics.h"

// Consumer for the plus_address bottom sheet. It is notified as required data
// elements become available.
@protocol PlusAddressBottomSheetConsumer

// Used to indicate the successful reservation of a plus address.
- (void)didReservePlusAddress:(NSString*)reservedPlusAddress;

// Used to indicate the successful confirmation of a plus address.
- (void)didConfirmPlusAddress;

// Used to indicate an error, specifying whether error occur during reservation
// or confirmation.
- (void)notifyError:
    (plus_addresses::metrics::PlusAddressModalCompletionStatus)status;

// Used to dismiss the bottom sheet.
- (void)dismissBottomSheet;

// Used to indicate to try again to confirm the plus address.
- (void)didSelectTryAgainToConfirm;

@end

#endif  // IOS_CHROME_BROWSER_PLUS_ADDRESSES_UI_PLUS_ADDRESS_BOTTOM_SHEET_CONSUMER_H_
