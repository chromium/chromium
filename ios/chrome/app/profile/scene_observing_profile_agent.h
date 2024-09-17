// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_PROFILE_SCENE_OBSERVING_PROFILE_AGENT_H_
#define IOS_CHROME_APP_PROFILE_SCENE_OBSERVING_PROFILE_AGENT_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/app/profile/observing_profile_agent.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_observer.h"

// A profile agent that observes the ProfileState and every connected scene.
// Provides some convenient synthetic events.
// Use this class when you want to observe some simple event that is made
// complicated because of the multiple windows, e.g. "the app is foreground".
// Extend this class with new events as necessary.
@interface SceneObservingProfileAgent
    : ObservingProfileAgent <SceneStateObserver>
@end

#endif  // IOS_CHROME_APP_PROFILE_SCENE_OBSERVING_PROFILE_AGENT_H_
