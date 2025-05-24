// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_BEST_FEATURES_UI_BEST_FEATURES_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_BEST_FEATURES_UI_BEST_FEATURES_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/first_run/ui_bundled/best_features/ui/best_features_screen_consumer.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"

@protocol BestFeaturesDelegate;

// View controller to present the Best Features screen, a fullscreen sheet in
// the First Run Experience containing a table view highlighting three of the
// "best" features.
@interface BestFeaturesViewController
    : PromoStyleViewController <BestFeaturesScreenConsumer>

// Delegate to communicate user interaction with the screen's table view.
@property(nonatomic, weak) id<BestFeaturesDelegate> bestFeaturesDelegate;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_BEST_FEATURES_UI_BEST_FEATURES_VIEW_CONTROLLER_H_
