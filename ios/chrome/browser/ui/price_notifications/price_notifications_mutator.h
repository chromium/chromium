// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_MUTATOR_H_
#define IOS_CHROME_BROWSER_UI_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_MUTATOR_H_

#import <Foundation/Foundation.h>

@class PriceNotificationsTableViewItem;

// Protocol to communicate price tracking actions to the mediator.
@protocol PriceNotificationsMutator

// Begins price tracking the `item`.
- (void)trackItem:(PriceNotificationsTableViewItem*)item;

// Stops price tracking the `item`.
- (void)stopTrackingItem:(PriceNotificationsTableViewItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_UI_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_MUTATOR_H_
