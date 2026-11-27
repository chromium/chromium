// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_ACCOUNT_SWITCHING_PROMO_MODEL_ACCOUNT_SWITCHING_PROMO_PROFILE_AGENT_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_ACCOUNT_SWITCHING_PROMO_MODEL_ACCOUNT_SWITCHING_PROMO_PROFILE_AGENT_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/app/profile/scene_observing_profile_agent.h"

@protocol SystemIdentity;

// Profile agent that evaluates smart account switching eligibility at Chrome
// startup, and registers `promos_manager::Promo::AccountSwitching` with
// `PromosManager` if eligible. `PromosManager` then decides when to show the
// promo. The agent deregisters the promo if it stops being eligible, prunes the
// promo state of accounts removed from the device, records the 24-hour
// retention metric of an accepted switch, and notifies the Feature Engagement
// Tracker when the primary account is set.
@interface AccountSwitchingPromoProfileAgent : SceneObservingProfileAgent

// The identity of this profile that the promo offers to switch to, if the
// profile is personal and has a non-supervised, non-managed signed-in primary
// account with a distinct non-managed preferred switching candidate, or `nil`
// otherwise. Used by the promo display handler to know which identity to show.
@property(nonatomic, readonly) id<SystemIdentity> targetIdentity;

// Resets the global flag tracking whether the promo was already evaluated at
// startup, for testing.
+ (void)resetPromoAlreadyEvaluatedForTesting;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_ACCOUNT_SWITCHING_PROMO_MODEL_ACCOUNT_SWITCHING_PROMO_PROFILE_AGENT_H_
