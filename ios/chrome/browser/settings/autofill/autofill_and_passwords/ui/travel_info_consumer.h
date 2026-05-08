// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_TRAVEL_INFO_CONSUMER_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_TRAVEL_INFO_CONSUMER_H_

#import <Foundation/Foundation.h>

@class TableViewItem;

// Consumer protocol for the Travel Info settings page.
@protocol TravelInfoConsumer <NSObject>

// Sets the list of travel info items.
- (void)setTravelInfoItems:(NSArray<TableViewItem*>*)travelInfoItems;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_TRAVEL_INFO_CONSUMER_H_
