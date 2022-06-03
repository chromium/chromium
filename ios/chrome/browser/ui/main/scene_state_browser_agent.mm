// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BROWSER_USER_DATA_KEY_IMPL(SceneStateBrowserAgent)

void SceneStateBrowserAgent::CreateForBrowser(Browser* browser,
                                              SceneState* scene_state) {
  DCHECK(browser);
  if (!FromBrowser(browser)) {
    browser->SetUserData(
        UserDataKey(),
        base::WrapUnique(new SceneStateBrowserAgent(browser, scene_state)));
  }
}

SceneStateBrowserAgent::~SceneStateBrowserAgent() {}

SceneState* SceneStateBrowserAgent::GetSceneState() {
  return scene_state_;
}

SceneStateBrowserAgent::SceneStateBrowserAgent(Browser* browser,
                                               SceneState* scene_state)
    : scene_state_(scene_state) {}
