// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"

#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_scene_agent.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_browser_agent.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

LayoutGuideCenter* SharedInstance() {
  static LayoutGuideCenter* globalLayoutGuideCenter;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    globalLayoutGuideCenter = [[LayoutGuideCenter alloc] init];
  });
  return globalLayoutGuideCenter;
}

}  // namespace

LayoutGuideCenter* LayoutGuideCenterForBrowser(Browser* browser) {
  if (!browser) {
    // If there is no browser, return a global layout guide center.
    return SharedInstance();
  }

  SceneStateBrowserAgent* sceneStateBrowserAgent =
      SceneStateBrowserAgent::FromBrowser(browser);
  if (!sceneStateBrowserAgent) {
    return SharedInstance();
  }

  SceneState* sceneState = sceneStateBrowserAgent->GetSceneState();
  LayoutGuideSceneAgent* layoutGuideSceneAgent =
      [LayoutGuideSceneAgent agentFromScene:sceneState];
  if (!layoutGuideSceneAgent) {
    return SharedInstance();
  }

  ChromeBrowserState* browserState = browser->GetBrowserState();
  if (browserState && browserState->IsOffTheRecord()) {
    return layoutGuideSceneAgent.incognitoLayoutGuideCenter;
  } else {
    return layoutGuideSceneAgent.layoutGuideCenter;
  }
}
