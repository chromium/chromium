// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INCOGNITO_REAUTH_INCOGNITO_REAUTH_SCENE_AGENT_H_
#define IOS_CHROME_BROWSER_UI_INCOGNITO_REAUTH_INCOGNITO_REAUTH_SCENE_AGENT_H_

#import "ios/chrome/browser/ui/main/scene_state.h"

class PrefRegistrySimple;
class PrefService;

// A scene agent that tracks the incognito authentication status for the current
// scene.
@interface IncognitoReauthSceneAgent : NSObject <SceneAgent>

// Registers the prefs required for this agent.
+ (void)registerLocalState:(PrefRegistrySimple*)registry;

// Returns YES when the authentication is currently required.
@property(nonatomic, assign, readonly, getter=isAuthenticationRequired)
    BOOL authenticationRequired;

// Local state pref service used by this object. Will default to the one from
// ApplicationContext, but is settable for overriding.
@property(nonatomic, assign) PrefService* localState;

// Requests authentication and marks this scene as authenticated until the next
// scene foregrounding. The callback will receive the result of the
// authentication attempt. It will be called on main thread, asynchronously.
- (void)authenticateWithCompletion:(void (^)(BOOL))completion;

@end

#endif  // IOS_CHROME_BROWSER_UI_INCOGNITO_REAUTH_INCOGNITO_REAUTH_SCENE_AGENT_H_
