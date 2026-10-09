// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_PROFILE_ACTOR_PROFILE_AGENT_H_
#define IOS_CHROME_APP_PROFILE_ACTOR_PROFILE_AGENT_H_

#import "ios/chrome/app/background_task/background_continued_processing_task_provider.h"
#import "ios/chrome/app/profile/observing_profile_agent.h"

// Profile agent bridging the profile's Actor Service and background continued
// processing: when the app enters the background, it requests background tasks
// from eligible background task workers of active Actor Tasks.
@interface ActorProfileAgent
    : ObservingProfileAgent <BackgroundContinuedProcessingTaskProvider>
@end

#endif  // IOS_CHROME_APP_PROFILE_ACTOR_PROFILE_AGENT_H_
