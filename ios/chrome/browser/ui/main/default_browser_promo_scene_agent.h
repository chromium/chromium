// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MAIN_DEFAULT_BROWSER_PROMO_SCENE_AGENT_H_
#define IOS_CHROME_BROWSER_UI_MAIN_DEFAULT_BROWSER_PROMO_SCENE_AGENT_H_

#import "ios/chrome/browser/promos_manager/promos_manager.h"
#import "ios/chrome/browser/shared/coordinator/scene/observing_scene_state_agent.h"

@class CommandDispatcher;

// A scene agent that shows the default browser fullscreen promo UI based on the
// SceneActivationLevel changes.
@interface DefaultBrowserPromoSceneAgent : ObservingSceneAgent

- (instancetype)initWithCommandDispatcher:(CommandDispatcher*)dispatcher;

// Command Dispatcher.
@property(nonatomic, weak) CommandDispatcher* dispatcher;

@property(nonatomic, assign) PromosManager* promosManager;

@end

#endif  // IOS_CHROME_BROWSER_UI_MAIN_DEFAULT_BROWSER_PROMO_SCENE_AGENT_H_
