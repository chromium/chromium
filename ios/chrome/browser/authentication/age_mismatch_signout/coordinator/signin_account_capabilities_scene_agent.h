// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_AGE_MISMATCH_SIGNOUT_COORDINATOR_SIGNIN_ACCOUNT_CAPABILITIES_SCENE_AGENT_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_AGE_MISMATCH_SIGNOUT_COORDINATOR_SIGNIN_ACCOUNT_CAPABILITIES_SCENE_AGENT_H_

#import "ios/chrome/browser/shared/coordinator/scene/observing_scene_state_agent.h"

@protocol SceneUIProvider;

// A scene agent that monitors the state of the app and handles account
// capabilities fetching.
@interface SigninAccountCapabilitiesSceneAgent : ObservingSceneAgent

- (instancetype)initWithSceneUIProvider:(id<SceneUIProvider>)sceneUIProvider;

// Returns YES if a sign-out is in progress or if the Age Mismatch prompt is
// being shown.
@property(nonatomic, readonly) BOOL isSignoutInProgress;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_AGE_MISMATCH_SIGNOUT_COORDINATOR_SIGNIN_ACCOUNT_CAPABILITIES_SCENE_AGENT_H_
