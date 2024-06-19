// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_CARD_CONSUMER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_CARD_CONSUMER_H_

#import <Foundation/Foundation.h>

@class ManualFillActionItem;
@class ManualFillCardItem;

// Objects conforming to this protocol need to react when new data is available.
// TODO(crbug.com/40577448): rename all class/file with 'Card' to 'CreditCard'.
@protocol ManualFillCardConsumer

// Tells the consumer to show the passed cards.
- (void)presentCards:(NSArray<ManualFillCardItem*>*)cards;

// Asks the consumer to present the passed actions
- (void)presentActions:(NSArray<ManualFillActionItem*>*)actions;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_CARD_CONSUMER_H_
