// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_PRICE_TRACKING_PROMO_UI_PRICE_TRACKING_PROMO_FAVICON_CONSUMER_SOURCE_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_PRICE_TRACKING_PROMO_UI_PRICE_TRACKING_PROMO_FAVICON_CONSUMER_SOURCE_H_

@protocol PriceTrackingPromoFaviconConsumer;

// Protocol for an object that provides favicon updates to consumers for the
// Price Tracking Promo.
@protocol PriceTrackingPromoFaviconConsumerSource

// Adds a `consumer` to receive updates on Price Tracking Promo favicons.
- (void)addConsumer:(id<PriceTrackingPromoFaviconConsumer>)consumer;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_PRICE_TRACKING_PROMO_UI_PRICE_TRACKING_PROMO_FAVICON_CONSUMER_SOURCE_H_
