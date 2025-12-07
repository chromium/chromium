// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_TEST_FAKE_SCENE_STATE_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_TEST_FAKE_SCENE_STATE_H_

#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/stub_browser_provider_interface.h"
#import "url/gurl.h"

class ProfileIOS;

// Test double for SceneState, created with appropriate interface objects backed
// by a browser. No incognito interface is created by default.
// Any test using objects of this class must include a TaskEnvironment member
// because of the embedded test profile.
@interface FakeSceneState : SceneState

// Initializer.
- (instancetype)initWithAppState:(AppState*)appState
                         profile:(ProfileIOS*)profile NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithAppState:(AppState*)appState NS_UNAVAILABLE;

// Redeclares interface provider as readwrite.
@property(nonatomic, strong, readwrite)
    StubBrowserProviderInterface* browserProviderInterface;

// Window for the associated scene, if any.
// This is redeclared relative to FakeScene.window, except this is now readwrite
// and backed by an instance variable.
@property(nonatomic, strong, readwrite) UIWindow* window;

// Redeclares appState as readwrite.
@property(nonatomic, weak, readwrite) AppState* appState;

// Appends a suitable web state test double to the receiver's main interface.
- (void)appendWebStateWithURL:(const GURL)URL;

// Appends `count` web states, all with `url` as the current URL, to the
- (void)appendWebStatesWithURL:(const GURL)URL count:(int)count;

// Must be called before -dealloc.
- (void)shutdown;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_TEST_FAKE_SCENE_STATE_H_
