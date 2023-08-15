// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/default_browser_promo/default_browser_promo_scene_agent_utils.h"

#import "ios/chrome/browser/default_browser/utils.h"
#import "ios/chrome/browser/shared/coordinator/default_browser_promo/non_modal_default_browser_promo_scheduler_scene_agent.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_browser_agent.h"
#import "ios/chrome/browser/ui/main/default_browser_promo_scene_agent.h"

void NotifyDefaultBrowserPromoUserPastedInOmnibox(SceneState* sceneState) {
  if (IsFullScreenPromoOnOmniboxCopyPasteEnabled()) {
    [[DefaultBrowserPromoSceneAgent agentFromScene:sceneState]
        logUserPastedInOmnibox];
  } else {
    [[NonModalDefaultBrowserPromoSchedulerSceneAgent agentFromScene:sceneState]
        logUserPastedInOmnibox];
  }
}
