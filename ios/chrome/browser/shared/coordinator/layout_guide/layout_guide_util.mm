// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_scene_agent.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"

namespace {

LayoutGuideCenter* SharedInstance() {
  NOTREACHED(base::NotFatalUntil::M155);
  static LayoutGuideCenter* globalLayoutGuideCenter;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    globalLayoutGuideCenter = [[LayoutGuideCenter alloc] init];
  });
  return globalLayoutGuideCenter;
}

}  // namespace

LayoutGuideCenter* LayoutGuideCenterForBrowser(Browser* browser) {
  CHECK(browser, base::NotFatalUntil::M155)
      << "Use the new Scene scoped center.";
  if (!browser) {
    // If there is no browser, return a global layout guide center.
    return SharedInstance();
  }

  SceneState* sceneState = browser->GetSceneState();
  LayoutGuideSceneAgent* layoutGuideSceneAgent =
      [LayoutGuideSceneAgent agentFromScene:sceneState];
  CHECK(layoutGuideSceneAgent, base::NotFatalUntil::M155);
  if (!layoutGuideSceneAgent) {
    return SharedInstance();
  }

  ProfileIOS* profile = browser->GetProfile();
  if (profile && profile->IsOffTheRecord()) {
    return layoutGuideSceneAgent.incognitoLayoutGuideCenter;
  } else {
    return layoutGuideSceneAgent.regularLayoutGuideCenter;
  }
}

LayoutGuideCenter* LayoutGuideCenterForScene(SceneState* sceneState) {
  LayoutGuideSceneAgent* layoutGuideSceneAgent =
      [LayoutGuideSceneAgent agentFromScene:sceneState];
  return layoutGuideSceneAgent.sceneLayoutGuideCenter;
}
