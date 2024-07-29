// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MINI_MAP_UI_BUNDLED_MINI_MAP_INTERSTITIAL_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_MINI_MAP_UI_BUNDLED_MINI_MAP_INTERSTITIAL_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/mini_map/ui_bundled/mini_map_action_handler.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"

// The view controller presenting the Consent screen for the Minimap feature.
@interface MiniMapInterstitialViewController : PromoStyleViewController

// The handler for UI interactions.
@property(nonatomic, weak) id<MiniMapActionHandler> actionHandler;

@end

#endif  // IOS_CHROME_BROWSER_MINI_MAP_UI_BUNDLED_MINI_MAP_INTERSTITIAL_VIEW_CONTROLLER_H_
