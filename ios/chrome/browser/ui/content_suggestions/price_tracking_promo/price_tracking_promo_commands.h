// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_PRICE_TRACKING_PROMO_PRICE_TRACKING_PROMO_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_PRICE_TRACKING_PROMO_PRICE_TRACKING_PROMO_COMMANDS_H_

// Command protocol for events for the Price Tracking Promo module.
@protocol PriceTrackingPromoCommands

// User taps 'Allow Price Tracking Notifications' on the Price
// Tracking Promo magic stack module.
- (void)allowPriceTrackingNotifications;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_PRICE_TRACKING_PROMO_PRICE_TRACKING_PROMO_COMMANDS_H_
