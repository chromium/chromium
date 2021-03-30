// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MAIN_POLICY_SIGNOUT_SCENE_AGENT_H_
#define IOS_CHROME_BROWSER_UI_MAIN_POLICY_SIGNOUT_SCENE_AGENT_H_

#import "ios/chrome/browser/ui/main/observing_scene_state_agent.h"

@class CommandDispatcher;

// A scene agent that shows the sign-out prompt (due to policy change) on the
// SceneActivationLevel changes.
@interface PolicySignoutSceneAgent : ObservingSceneAgent

- (instancetype)initWithCommandDispatcher:(CommandDispatcher*)dispatcher;

@end

#endif  // IOS_CHROME_BROWSER_UI_MAIN_POLICY_SIGNOUT_SCENE_AGENT_H_
