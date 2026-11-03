// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_ACCOUNT_SWITCHING_PROMO_MODEL_ACCOUNT_SWITCHING_PROMO_SCENE_AGENT_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_ACCOUNT_SWITCHING_PROMO_MODEL_ACCOUNT_SWITCHING_PROMO_SCENE_AGENT_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/coordinator/scene/observing_scene_state_agent.h"

class ProfileIOS;

// Scene agent of the smart account switching promo. It notifies the Feature
// Engagement Tracker when the primary account is set.
@interface AccountSwitchingPromoSceneAgent : ObservingSceneAgent

// Returns a new `AccountSwitchingPromoSceneAgent` if the smart account
// switching promo is enabled and supported for `profile` (which must be
// non-null), or `nil` otherwise.
+ (instancetype)agentWithProfile:(ProfileIOS*)profile;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_ACCOUNT_SWITCHING_PROMO_MODEL_ACCOUNT_SWITCHING_PROMO_SCENE_AGENT_H_
