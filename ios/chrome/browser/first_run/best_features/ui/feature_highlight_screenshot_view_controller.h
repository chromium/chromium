// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_BEST_FEATURES_UI_FEATURE_HIGHLIGHT_SCREENSHOT_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_BEST_FEATURES_UI_FEATURE_HIGHLIGHT_SCREENSHOT_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/first_run/public/best_features_item.h"
#import "ios/chrome/browser/shared/ui/animated_promo/animated_promo_with_bricks_view_controller.h"

// View for displaying a BestFeaturesItem.
@interface FeatureHighlightScreenshotViewController
    : AnimatedPromoWithBricksViewController

// Creates the view with `BestFeaturesItem`.
- (instancetype)initWithFeatureHighlightItem:(BestFeaturesItem*)bestFeaturesItem
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithConfiguration:(AnimatedPromoConfiguration*)config
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_BEST_FEATURES_UI_FEATURE_HIGHLIGHT_SCREENSHOT_VIEW_CONTROLLER_H_
