// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MAIN_UI_BLOCKER_SCENE_AGENT_H_
#define IOS_CHROME_BROWSER_UI_MAIN_UI_BLOCKER_SCENE_AGENT_H_

#import "ios/chrome/browser/shared/coordinator/scene/observing_scene_state_agent.h"

// A scene agent that shows a UI overlay on the scene based on modal overlay
// show/hide events.
@interface UIBlockerSceneAgent : ObservingSceneAgent
@end

#endif  // IOS_CHROME_BROWSER_UI_MAIN_UI_BLOCKER_SCENE_AGENT_H_
