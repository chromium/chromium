// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_UI_BUNDLED_USER_POLICY_SCENE_AGENT_H_
#define IOS_CHROME_BROWSER_POLICY_UI_BUNDLED_USER_POLICY_SCENE_AGENT_H_

#import "ios/chrome/browser/shared/coordinator/scene/observing_scene_state_agent.h"

@protocol SceneUIProvider;
class AuthenticationService;
@protocol ApplicationCommands;
class PrefService;
class Browser;

namespace policy {
class UserCloudPolicyManager;
class UserPolicySigninService;
}  // namespace policy

// Scene agent that monitors app and scene events to control the UI for User
// Policy (e.g. show the User Policy notification dialog at startup).
//
// --
// User Policy notification Dialog:
//
// The scene agent will show the notification dialog if needed (ie. browser
// syncing with a managed account and the notification was never shown
// before).
//
// The agent will show the dialog when the UI of the scene is initialized which
// corresponds to the moment where the scene is active in the foreground,
// the app is at the AppInitStage::kFinal stage, and the UI to present the
// notification on is able to present views. Ideally there shouldn't be modals
// before showing the dialog, but if it happens the modals will be dismissed to
// make sure that the user has the opportunity to see the notification.
//
// The dialog will hide the banners (e.g. the restore banner) that are shown in
// the content. The user will be able to interact with these banners if they
// dismiss the dialog quickly enough before the banner times out and is auto
// dismissed.
//
// When there is more than one window, the agent will wait until one of the
// windows (aka scene) is visible and active. The other windows that aren't
// showing the dialog will have their UI blocked with a UI blocker until the
// user has made an action on the dialog.
//
// If the dialog is dismissed when shutting down the app, the dialog will be
// reshown at the next startup until the user makes an action on the dialog. In
// a multi-window context, if the user closes the window that is showing the
// dialog, the dialog will be shown on another window that is active (can be
// any of the other windows).
//--
@interface UserPolicySceneAgent : ObservingSceneAgent

- (instancetype)init NS_UNAVAILABLE;
// Initialize the scene state agent with a `sceneUIProvider` to provide the UI
// objects of the scene.
- (instancetype)initWithSceneUIProvider:(id<SceneUIProvider>)sceneUIProvider
                            authService:(AuthenticationService*)authService
             applicationCommandsHandler:
                 (id<ApplicationCommands>)applicationCommandsHandler
                            prefService:(PrefService*)prefService
                            mainBrowser:(Browser*)mainBrowser
                          policyService:
                              (policy::UserPolicySigninService*)policyService
                      userPolicyManager:
                          (policy::UserCloudPolicyManager*)userPolicyManager
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_POLICY_UI_BUNDLED_USER_POLICY_SCENE_AGENT_H_
