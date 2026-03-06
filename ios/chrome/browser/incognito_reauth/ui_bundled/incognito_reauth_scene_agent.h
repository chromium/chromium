// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INCOGNITO_REAUTH_UI_BUNDLED_INCOGNITO_REAUTH_SCENE_AGENT_H_
#define IOS_CHROME_BROWSER_INCOGNITO_REAUTH_UI_BUNDLED_INCOGNITO_REAUTH_SCENE_AGENT_H_

#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_commands.h"
#import "ios/chrome/browser/shared/coordinator/scene/observing_scene_state_agent.h"

enum class IncognitoLockState;
@class IncognitoReauthSceneAgent;
class PrefRegistrySimple;
class PrefService;
@protocol ReauthenticationProtocol;
@protocol SceneCommands;

// A scene agent that tracks the incognito authentication status for the current
// scene.
@interface IncognitoReauthSceneAgent
    : ObservingSceneAgent <IncognitoReauthCommands>

// Designated initializer.
// The `reauthModule` is used for authentication.
// The `sceneHandler` is used to transition between the tab and
// tab switcher.
- (instancetype)initWithReauthModule:(id<ReauthenticationProtocol>)reauthModule
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Requests authentication and marks the scene as authenticated until the next
// scene foregrounding.
// The authentication will require user interaction. Upon completion, will
// notify observers and call the completion block (passing authentication
// result).
- (void)authenticateIncognitoContentWithCompletionBlock:
    (void (^)(BOOL success))completion;

// Registers the prefs required for this agent.
+ (void)registerLocalState:(PrefRegistrySimple*)registry;

// Authentication module used when the user toggles the biometric auth on.
@property(nonatomic, strong, readonly) id<ReauthenticationProtocol>
    reauthModule;

// Local state pref service used by this object. Will default to the one from
// ApplicationContext, but is settable for overriding.
@property(nonatomic, assign) PrefService* localState;

@end

#endif  // IOS_CHROME_BROWSER_INCOGNITO_REAUTH_UI_BUNDLED_INCOGNITO_REAUTH_SCENE_AGENT_H_
