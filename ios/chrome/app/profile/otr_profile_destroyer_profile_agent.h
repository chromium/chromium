// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_PROFILE_OTR_PROFILE_DESTROYER_PROFILE_AGENT_H_
#define IOS_CHROME_APP_PROFILE_OTR_PROFILE_DESTROYER_PROFILE_AGENT_H_

#import "ios/chrome/app/profile/profile_state_agent.h"

// ProfileAgent responsible for destroying and recreating the OTR profile
// when the last incognito tab is closed.
@interface OTRPRofileDestroyerProfileAgent : NSObject <ProfileStateAgent>
@end

#endif  // IOS_CHROME_APP_PROFILE_OTR_PROFILE_DESTROYER_PROFILE_AGENT_H_
