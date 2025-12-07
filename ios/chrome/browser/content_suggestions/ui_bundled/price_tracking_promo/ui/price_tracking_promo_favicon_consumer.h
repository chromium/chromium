// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_PRICE_TRACKING_PROMO_UI_PRICE_TRACKING_PROMO_FAVICON_CONSUMER_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_PRICE_TRACKING_PROMO_UI_PRICE_TRACKING_PROMO_FAVICON_CONSUMER_H_

// Protocol for UI elements that consume the favicon image for the Price
// Tracking Promo.
@protocol PriceTrackingPromoFaviconConsumer

// Called when the favicon image for the Price Tracking Promo has been
// successfully fetched.
- (void)priceTrackingPromoFaviconCompleted:(UIImage*)faviconImage;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_PRICE_TRACKING_PROMO_UI_PRICE_TRACKING_PROMO_FAVICON_CONSUMER_H_
