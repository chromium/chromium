// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_MODEL_HOME_BACKGROUND_CUSTOMIZATION_PROMO_PROFILE_AGENT_H_
#define IOS_CHROME_BROWSER_NTP_MODEL_HOME_BACKGROUND_CUSTOMIZATION_PROMO_PROFILE_AGENT_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/app/profile/observing_profile_agent.h"

// A profile agent that registers/deregisters the Home Background Customization
// promo when necessary.
@interface HomeBackgroundCustomizationPromoProfileAgent : ObservingProfileAgent

@end

#endif  // IOS_CHROME_BROWSER_NTP_MODEL_HOME_BACKGROUND_CUSTOMIZATION_PROMO_PROFILE_AGENT_H_
