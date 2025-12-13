// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_BEST_FEATURES_UI_FEATURE_HIGHLIGHT_SCREENSHOT_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_BEST_FEATURES_UI_FEATURE_HIGHLIGHT_SCREENSHOT_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/first_run/public/best_features_item.h"
#import "ios/chrome/browser/shared/ui/animated_promo/animated_promo_view_controller.h"

@protocol ConfirmationAlertActionHandler;

// View for displaying a BestFeaturesItem.
@interface FeatureHighlightScreenshotViewController
    : AnimatedPromoViewController <UINavigationControllerDelegate>

// Creates the view with `BestFeaturesItem` and `actionHandler`.
- (instancetype)initWithFeatureHighlightItem:
    (BestFeaturesItem*)bestFeaturesItem;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_BEST_FEATURES_UI_FEATURE_HIGHLIGHT_SCREENSHOT_VIEW_CONTROLLER_H_
