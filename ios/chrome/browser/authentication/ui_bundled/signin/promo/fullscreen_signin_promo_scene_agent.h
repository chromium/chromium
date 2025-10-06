// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_PROMO_FULLSCREEN_SIGNIN_PROMO_SCENE_AGENT_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_PROMO_FULLSCREEN_SIGNIN_PROMO_SCENE_AGENT_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/coordinator/scene/observing_scene_state_agent.h"

namespace signin {
class IdentityManager;
}

namespace syncer {
class SyncService;
}

class PromosManager;
class AuthenticationService;
class PrefService;

// A scene agent that registers the Fullscreen Signin promo in the promo
// manager.
@interface FullscreenSigninPromoSceneAgent : ObservingSceneAgent

- (instancetype)initWithPromosManager:(PromosManager*)promosManager
                          authService:(AuthenticationService*)authService
                      identityManager:(signin::IdentityManager*)identityManager
                          syncService:(syncer::SyncService*)syncService
                          prefService:(PrefService*)prefService;
@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_PROMO_FULLSCREEN_SIGNIN_PROMO_SCENE_AGENT_H_
