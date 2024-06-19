// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_ADDRESS_CONSUMER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_ADDRESS_CONSUMER_H_

#import <Foundation/Foundation.h>

@class ManualFillActionItem;
@class ManualFillAddressItem;

// Objects conforming to this protocol need to react when new data is available.
@protocol ManualFillAddressConsumer

// Tells the consumer to show the passed addreses.
- (void)presentAddresses:(NSArray<ManualFillAddressItem*>*)addresses;

// Asks the consumer to present the passed actions
- (void)presentActions:(NSArray<ManualFillActionItem*>*)actions;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_ADDRESS_CONSUMER_H_
