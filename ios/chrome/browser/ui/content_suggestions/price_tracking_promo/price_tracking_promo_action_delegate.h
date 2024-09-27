// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_PRICE_TRACKING_PROMO_PRICE_TRACKING_PROMO_ACTION_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_PRICE_TRACKING_PROMO_PRICE_TRACKING_PROMO_ACTION_DELEGATE_H_

// Protocol for delegating actions to the owner of the
// PriceTrackingPromoMediator
@protocol PriceTrackingPromoActionDelegate

// Show alert giving user the option of turning on notifications for the app.
- (void)showPriceTrackingPromoAlertCoordinator;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_PRICE_TRACKING_PROMO_PRICE_TRACKING_PROMO_ACTION_DELEGATE_H_
