// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_STORE_RATING_UI_BUNDLED_APP_STORE_RATING_SCENE_AGENT_H_
#define IOS_CHROME_BROWSER_APP_STORE_RATING_UI_BUNDLED_APP_STORE_RATING_SCENE_AGENT_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/coordinator/scene/observing_scene_state_agent.h"

class PromosManager;

// A scene agent that requests engaged users are presented the
// App Store Rating promo based on the SceneActivationLevel changes.
@interface AppStoreRatingSceneAgent : ObservingSceneAgent

// Initializes an AppStoreRatingSceneAgent instance with given PromosManager.
- (instancetype)initWithPromosManager:(PromosManager*)promosManager;

// Unavailable. Use initWithPromosManager:.
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_APP_STORE_RATING_UI_BUNDLED_APP_STORE_RATING_SCENE_AGENT_H_
