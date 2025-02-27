// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_PROMO_SIGNIN_FULLSCREEN_PROMO_SCENE_AGENT_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_PROMO_SIGNIN_FULLSCREEN_PROMO_SCENE_AGENT_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/coordinator/scene/observing_scene_state_agent.h"

class PromosManager;

// A scene agent that registers the Signin fullscreen promo in the promo
// manager.
@interface SigninFullscreenPromoSceneAgent : ObservingSceneAgent

- (instancetype)initWithPromosManager:(PromosManager*)promosManager;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_PROMO_SIGNIN_FULLSCREEN_PROMO_SCENE_AGENT_H_
