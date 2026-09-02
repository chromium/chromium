// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_SHOPPING_CONSUMER_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_SHOPPING_CONSUMER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/suggestions_from_gemini_entry_point_consumer.h"

@class TableViewItem;

// Consumer protocol for Shopping settings.
@protocol ShoppingConsumer <SuggestionsFromGeminiEntryPointConsumer>

// Sets the shopping item list with orders and shipments.
- (void)setShoppingWithOrders:(NSArray<TableViewItem*>*)orders
                    shipments:(NSArray<TableViewItem*>*)shipments;

// Sets the toggle state for "fill shopping info", its enabled and managed
// states.
- (void)setShoppingToggleState:(BOOL)on
                       enabled:(BOOL)enabled
                       managed:(BOOL)managed;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_SHOPPING_CONSUMER_H_
