// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/scene/scene_state_browser_agent.h"

BROWSER_USER_DATA_KEY_IMPL(SceneStateBrowserAgent)

SceneStateBrowserAgent::~SceneStateBrowserAgent() {}

SceneState* SceneStateBrowserAgent::GetSceneState() {
  return scene_state_;
}

SceneStateBrowserAgent::SceneStateBrowserAgent(Browser* browser,
                                               SceneState* scene_state)
    : scene_state_(scene_state) {}
