// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/layout_guide_util.h"

#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/main/layout_guide_scene_agent.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

LayoutGuideCenter* LayoutGuideCenterForBrowser(Browser* browser) {
  SceneState* sceneState =
      SceneStateBrowserAgent::FromBrowser(browser)->GetSceneState();
  LayoutGuideSceneAgent* layoutGuideSceneAgent =
      [LayoutGuideSceneAgent agentFromScene:sceneState];
  ChromeBrowserState* browserState = browser->GetBrowserState();
  if (browserState && browserState->IsOffTheRecord()) {
    return layoutGuideSceneAgent.incognitoLayoutGuideCenter;
  } else {
    return layoutGuideSceneAgent.layoutGuideCenter;
  }
}
