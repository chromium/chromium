// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_PROFILE_APP_ICON_LAUNCHED_PROFILE_AGENT_H_
#define IOS_CHROME_APP_PROFILE_APP_ICON_LAUNCHED_PROFILE_AGENT_H_

#import "ios/chrome/app/profile/scene_observing_profile_agent.h"

// A profile agent that notifies the Feature Engagement Tracker about
// events related to the profile lifecycle and usage.
@interface AppIconLaunchedProfileAgent : SceneObservingProfileAgent

@end

#endif  // IOS_CHROME_APP_PROFILE_APP_ICON_LAUNCHED_PROFILE_AGENT_H_
