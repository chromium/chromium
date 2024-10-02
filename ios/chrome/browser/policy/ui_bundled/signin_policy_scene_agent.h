// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_UI_BUNDLED_SIGNIN_POLICY_SCENE_AGENT_H_
#define IOS_CHROME_BROWSER_POLICY_UI_BUNDLED_SIGNIN_POLICY_SCENE_AGENT_H_

#import "ios/chrome/browser/shared/coordinator/scene/observing_scene_state_agent.h"

@protocol SceneUIProvider;
@protocol ApplicationCommands;
@protocol PolicyChangeCommands;

// A scene agent that monitors the state of the app and policy updates to show
// the sign-out and sign-in prompts. Will show prompts when determined to be
// needed the UI of the scene is available (i.e., the states of the app and
// the scene allow it).
//
// The UI is considered as available when the following conditions are met:
// (1) the app initialization is over (the stage AppInitStage::kFinal is
// reached), (2) the scene is in the foreground, (3) there is no UI blocker, and
// (4) the app isn't shutting down.
//
// There are 4 events that can trigger the prompt: (1) a policy update, (2)
// reaching the AppInitStage::kFinal init stage, (3) the scene becomes active in
// the foreground, and (4) the UI blocker is removed.
//
// In a multi-window context, only one scene will present the prompt. The first
// scene to receive the event that triggers the prompt will be the one selected.
// Another scene will be selected if the presenting scene is dismissed.
@interface SigninPolicySceneAgent : ObservingSceneAgent

- (instancetype)initWithSceneUIProvider:(id<SceneUIProvider>)sceneUIProvider
             applicationCommandsHandler:
                 (id<ApplicationCommands>)applicationCommandsHandle
            policyChangeCommandsHandler:
                (id<PolicyChangeCommands>)policyChangeCommandsHandler;

@end

#endif  // IOS_CHROME_BROWSER_POLICY_UI_BUNDLED_SIGNIN_POLICY_SCENE_AGENT_H_
