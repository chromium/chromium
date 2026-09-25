// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LEVEL_UP_MODEL_LEVEL_UP_PROMO_PROFILE_AGENT_H_
#define IOS_CHROME_BROWSER_LEVEL_UP_MODEL_LEVEL_UP_PROMO_PROFILE_AGENT_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/app/profile/observing_profile_agent.h"

// A ProfileIOSAgent that registers the Level Up promo with the PromosManager
// when the user reaches the required number of active days.
@interface LevelUpPromoProfileAgent : ObservingProfileAgent

@end

#endif  // IOS_CHROME_BROWSER_LEVEL_UP_MODEL_LEVEL_UP_PROMO_PROFILE_AGENT_H_
