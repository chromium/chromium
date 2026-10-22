// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_PROFILE_FIRST_RUN_PROFILE_AGENT_H_
#define IOS_CHROME_APP_PROFILE_FIRST_RUN_PROFILE_AGENT_H_

#import "ios/chrome/app/profile/observing_profile_agent.h"

// ProfileAgent that displays the first run UI when needed and handles the
// ProfileInitStage::kFirstRun stage (including the transition to next stage).
@interface FirstRunProfileAgent : ObservingProfileAgent
@end

#endif  // IOS_CHROME_APP_PROFILE_FIRST_RUN_PROFILE_AGENT_H_
