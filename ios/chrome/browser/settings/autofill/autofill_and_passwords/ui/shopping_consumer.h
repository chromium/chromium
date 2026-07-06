// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_SHOPPING_CONSUMER_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_SHOPPING_CONSUMER_H_

#import <Foundation/Foundation.h>

@class TableViewItem;

// Consumer protocol for Shopping settings.
@protocol ShoppingConsumer <NSObject>

// Sets the shopping item list with orders and shipments.
- (void)setShoppingWithOrders:(NSArray<TableViewItem*>*)orders
                    shipments:(NSArray<TableViewItem*>*)shipments;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_SHOPPING_CONSUMER_H_
