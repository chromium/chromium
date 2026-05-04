// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_BEST_FEATURES_UI_FEATURE_HIGHLIGHT_ANIMATED_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_BEST_FEATURES_UI_FEATURE_HIGHLIGHT_ANIMATED_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/first_run/public/best_features_item.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"

// View controller to present animated feature promos. A fullscreen sheet in the
// First Run Experience that contains an animation showing how to use the
// targeted feature.
@interface FeatureHighlightAnimatedViewController : PromoStyleViewController

// Creates the view with `BestFeaturesItem`, starting at the specified index.
- (instancetype)initWithFeatureHighlightItem:(BestFeaturesItem*)bestFeaturesItem
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_BEST_FEATURES_UI_FEATURE_HIGHLIGHT_ANIMATED_VIEW_CONTROLLER_H_
