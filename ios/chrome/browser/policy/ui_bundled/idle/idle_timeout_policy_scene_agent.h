// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_UI_BUNDLED_IDLE_IDLE_TIMEOUT_POLICY_SCENE_AGENT_H_
#define IOS_CHROME_BROWSER_POLICY_UI_BUNDLED_IDLE_IDLE_TIMEOUT_POLICY_SCENE_AGENT_H_

#import "ios/chrome/browser/shared/coordinator/scene/observing_scene_state_agent.h"

#import "ios/chrome/browser/enterprise/model/idle/idle_service.h"

@protocol ApplicationCommands;
@protocol SceneUIProvider;
@protocol SnackbarCommands;
class Browser;

// Scene agent that acts as an IdleTimeoutObserver to update the UI when the
// browser times out and when the idle timeout actions run. See
// `IdleTimeout.yaml`. The agent is responsible for updating the UI in the
// following cases:
// 1. When the browser times out while it is in foreground, and it needs to show
// a dialog for 30s to give the user a chance to respond if they are not idle.
// 2. To show a snackbar after IdleTimeoutActions have run.
// 3. To show a loading window on start-up if data is being cleared.
// 4. To dismiss the timeout confirmation dialog when the app is backgrounded
// with no scenes left in foreground.
@interface IdleTimeoutPolicySceneAgent : ObservingSceneAgent

- (instancetype)
       initWithSceneUIProvider:(id<SceneUIProvider>)sceneUIProvider
    applicationCommandsHandler:(id<ApplicationCommands>)applicationHandler
       snackbarCommandsHandler:(id<SnackbarCommands>)snackbarHandler
                   idleService:(enterprise_idle::IdleService*)idleService
                   mainBrowser:(Browser*)mainBrowser;

@end

#endif  // IOS_CHROME_BROWSER_POLICY_UI_BUNDLED_IDLE_IDLE_TIMEOUT_POLICY_SCENE_AGENT_H_
