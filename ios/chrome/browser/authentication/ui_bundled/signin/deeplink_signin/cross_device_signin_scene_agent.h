// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_DEEPLINK_SIGNIN_CROSS_DEVICE_SIGNIN_SCENE_AGENT_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_DEEPLINK_SIGNIN_CROSS_DEVICE_SIGNIN_SCENE_AGENT_H_

#import "ios/chrome/browser/shared/coordinator/scene/observing_scene_state_agent.h"

class SceneUrlLoadingService;
@protocol SceneCommands;

// A scene agent that registers the CrossDeviceSigninURLInterceptor to intercept
// cross-device sign-in URLs and trigger the sign-in flow.
@interface CrossDeviceSigninSceneAgent : ObservingSceneAgent

// Designated initializer.
- (instancetype)initWithSceneURLLoadingService:
                    (SceneUrlLoadingService*)sceneURLLoadingService
                                  sceneHandler:(id<SceneCommands>)sceneHandler
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_DEEPLINK_SIGNIN_CROSS_DEVICE_SIGNIN_SCENE_AGENT_H_
