// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_PROMO_SEARCH_ENGINE_CHOICE_SCENE_AGENT_H_
#define IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_PROMO_SEARCH_ENGINE_CHOICE_SCENE_AGENT_H_

#import "ios/chrome/browser/shared/coordinator/scene/observing_scene_state_agent.h"

// TODO(b/306576460): Clean up all promos manager -related code for the choice
// screen once the internal references to this class are deleted.

class ChromeBrowserState;
class PromosManager;

// A scene agent that registers the search engine choice screen in the promo
// manager on SceneActivationLevelForegroundActive.
@interface SearchEngineChoiceSceneAgent : ObservingSceneAgent

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithPromosManager:(PromosManager*)promosManager
                      forBrowserState:(ChromeBrowserState*)browserState
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_PROMO_SEARCH_ENGINE_CHOICE_SCENE_AGENT_H_
