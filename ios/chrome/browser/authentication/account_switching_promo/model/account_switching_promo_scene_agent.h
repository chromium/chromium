// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_ACCOUNT_SWITCHING_PROMO_MODEL_ACCOUNT_SWITCHING_PROMO_SCENE_AGENT_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_ACCOUNT_SWITCHING_PROMO_MODEL_ACCOUNT_SWITCHING_PROMO_SCENE_AGENT_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/coordinator/scene/observing_scene_state_agent.h"

class ProfileIOS;
@protocol SystemIdentity;

// Scene agent that evaluates smart account switching eligibility at Chrome
// startup, and registers `promos_manager::Promo::AccountSwitching` with
// `PromosManager` if eligible. `PromosManager` then decides when to show the
// promo. The agent deregisters the promo if it stops being eligible, prunes the
// promo state of accounts removed from the device, records the 24-hour
// retention metric of an accepted switch, and notifies the Feature Engagement
// Tracker when the primary account is set.
@interface AccountSwitchingPromoSceneAgent : ObservingSceneAgent

// The identity of this profile that the promo offers to switch to, if the
// profile is personal and has a non-supervised, non-managed signed-in primary
// account with a distinct preferred switching candidate, or `nil` otherwise.
// Used by the promo display handler to know which identity to show. Must only
// be accessed while the scene UI is enabled.
@property(nonatomic, readonly) id<SystemIdentity> targetIdentity;

// Returns a new `AccountSwitchingPromoSceneAgent` if the smart account
// switching promo is enabled and supported for `profile` (which must be
// non-null), or `nil` otherwise.
+ (instancetype)agentWithProfile:(ProfileIOS*)profile;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_ACCOUNT_SWITCHING_PROMO_MODEL_ACCOUNT_SWITCHING_PROMO_SCENE_AGENT_H_
