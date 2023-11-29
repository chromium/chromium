// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PLUS_ADDRESSES_UI_PLUS_ADDRESS_BOTTOM_SHEET_CONSUMER_H_
#define IOS_CHROME_BROWSER_PLUS_ADDRESSES_UI_PLUS_ADDRESS_BOTTOM_SHEET_CONSUMER_H_

#import <Foundation/Foundation.h>

// Consumer for the plus_address bottom sheet. It is notified as required data
// elements become available.
@protocol PlusAddressBottomSheetConsumer

// Used to indicate the successful reservation of a plus address.
- (void)didReservePlusAddress:(NSString*)reservedPlusAddress;

// Used to indicate the successful confirmation of a plus address.
- (void)didConfirmPlusAddress;

// Used to indicate an error, whether with reservation or confirmation.
- (void)notifyError;

@end

#endif  // IOS_CHROME_BROWSER_PLUS_ADDRESSES_UI_PLUS_ADDRESS_BOTTOM_SHEET_DELEGATE_H_
