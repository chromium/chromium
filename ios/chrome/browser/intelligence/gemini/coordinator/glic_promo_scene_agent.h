// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_GEMINI_COORDINATOR_GLIC_PROMO_SCENE_AGENT_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_GEMINI_COORDINATOR_GLIC_PROMO_SCENE_AGENT_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/coordinator/scene/observing_scene_state_agent.h"

class PromosManager;

// A scene agent that registers the GLIC promo in the promo manager on
// SceneActivationLevelForegroundActive.
@interface GLICPromoSceneAgent : ObservingSceneAgent

- (instancetype)initWithPromosManager:(PromosManager*)promosManager;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_GEMINI_COORDINATOR_GLIC_PROMO_SCENE_AGENT_H_
