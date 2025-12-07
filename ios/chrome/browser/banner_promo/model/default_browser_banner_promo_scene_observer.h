// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BANNER_PROMO_MODEL_DEFAULT_BROWSER_BANNER_PROMO_SCENE_OBSERVER_H_
#define IOS_CHROME_BROWSER_BANNER_PROMO_MODEL_DEFAULT_BROWSER_BANNER_PROMO_SCENE_OBSERVER_H_

#import <optional>

#import "ios/chrome/browser/shared/coordinator/scene/scene_state_observer.h"

@class DefaultBrowserBannerPromoAppAgent;
class GURL;
@class SceneState;

// Observer for an individual scene, web state list, and active web state  for
// the Default Browser Banner Promo.
@interface DefaultBrowserBannerPromoSceneObserver
    : NSObject <SceneStateObserver>

- (instancetype)initWithSceneState:(SceneState*)sceneState
                          appAgent:(DefaultBrowserBannerPromoAppAgent*)appAgent;

// The last URL that the current active web state navigated to, if one exists.
@property(nonatomic) std::optional<GURL> lastNavigatedURL;

@end

#endif  // IOS_CHROME_BROWSER_BANNER_PROMO_MODEL_DEFAULT_BROWSER_BANNER_PROMO_SCENE_OBSERVER_H_
