// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_PRICE_TRACKING_PROMO_PRICE_TRACKING_PROMO_VIEW_TESTING_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_PRICE_TRACKING_PROMO_PRICE_TRACKING_PROMO_VIEW_TESTING_H_

#import "ios/chrome/browser/content_suggestions/ui_bundled/price_tracking_promo/price_tracking_promo_view.h"

// Category for exposing internal state for testing.
@interface PriceTrackingPromoModuleView (ForTesting)

- (NSString*)titleLabelTextForTesting;

- (NSString*)descriptionLabelTextForTesting;

- (NSString*)allowLabelTextForTesting;

- (void)addConstraintsForProductImageForTesting;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_PRICE_TRACKING_PROMO_PRICE_TRACKING_PROMO_VIEW_TESTING_H_
