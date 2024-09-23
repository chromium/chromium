// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_PLUS_ADDRESS_CONSUMER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_PLUS_ADDRESS_CONSUMER_H_

#import <Foundation/Foundation.h>

@class ManualFillActionItem;
@class ManualFillPlusAddressItem;

// Objects conforming to this protocol need to react when new data is available.
@protocol ManualFillPlusAddressConsumer

// Tells the consumer to show the passed plus addresses.
- (void)presentPlusAddresses:
    (NSArray<ManualFillPlusAddressItem*>*)plusAddresses;

// Tells the consumer to present the passed plus address actions.
- (void)presentPlusAddressActions:(NSArray<ManualFillActionItem*>*)actions;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_PLUS_ADDRESS_CONSUMER_H_
