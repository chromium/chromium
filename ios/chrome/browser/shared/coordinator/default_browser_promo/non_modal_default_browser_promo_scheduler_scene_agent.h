// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_DEFAULT_BROWSER_PROMO_NON_MODAL_DEFAULT_BROWSER_PROMO_SCHEDULER_SCENE_AGENT_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_DEFAULT_BROWSER_PROMO_NON_MODAL_DEFAULT_BROWSER_PROMO_SCHEDULER_SCENE_AGENT_H_

#import "ios/chrome/browser/shared/coordinator/default_browser_promo/base_default_browser_promo_scene_agent.h"

// A scene-agent scheduler that determines when to show the non-modal default
// browser promo based on many sources of data.
@interface NonModalDefaultBrowserPromoSchedulerSceneAgent
    : BaseDefaultBrowserPromoSchedulerSceneAgent

@end

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_DEFAULT_BROWSER_PROMO_NON_MODAL_DEFAULT_BROWSER_PROMO_SCHEDULER_SCENE_AGENT_H_
