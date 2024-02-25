// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_OMNIBOX_POSITION_PROMO_OMNIBOX_POSITION_CHOICE_SCENE_AGENT_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_OMNIBOX_POSITION_PROMO_OMNIBOX_POSITION_CHOICE_SCENE_AGENT_H_

#import "ios/chrome/browser/shared/coordinator/scene/observing_scene_state_agent.h"

class ChromeBrowserState;
class PromosManager;

/// A scene agent that registers the omnibox position choice screen in the promo
/// manager on SceneActivationLevelForeground.
@interface OmniboxPositionChoiceSceneAgent : ObservingSceneAgent

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithPromosManager:(PromosManager*)promosManager
                      forBrowserState:(ChromeBrowserState*)browserState
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_OMNIBOX_POSITION_PROMO_OMNIBOX_POSITION_CHOICE_SCENE_AGENT_H_
