// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_LAYOUT_GUIDE_LAYOUT_GUIDE_SCENE_AGENT_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_LAYOUT_GUIDE_LAYOUT_GUIDE_SCENE_AGENT_H_

#import "ios/chrome/browser/shared/coordinator/scene/observing_scene_state_agent.h"

@class LayoutGuideCenter;

// A scene agent that scopes a LayoutGuideCenter to a scene.
@interface LayoutGuideSceneAgent : ObservingSceneAgent

// The layout guide center for the current scene and regular profile.
@property(nonatomic, readonly) LayoutGuideCenter* layoutGuideCenter;

// The layout guide center for the current scene and Incognito profile.
@property(nonatomic, readonly) LayoutGuideCenter* incognitoLayoutGuideCenter;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_LAYOUT_GUIDE_LAYOUT_GUIDE_SCENE_AGENT_H_
