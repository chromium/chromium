// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_TEST_FAKE_SCENE_STATE_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_TEST_FAKE_SCENE_STATE_H_

#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "url/gurl.h"

@protocol BrowserProvider;
class ProfileIOS;

// Test double for SceneState that create a regular and an incognito Browsers
// and connect them to be accessible via the BrowserProviderInterface.
@interface FakeSceneState : SceneState

// Designated initializer. The `profile` must not be null.
- (instancetype)initWithProfile:(ProfileIOS*)profile NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Updates the current BrowserProvider. Must be either -mainBrowserProvider
// or -incognitoBrowserProvider from -browserProviderInterface.
- (void)setCurrentBrowserProvider:(id<BrowserProvider>)browserProvider;

// Destroys and recreates the off-the-record Profile and Browser.
- (void)destroyAndRecreateOffTheRecordProfile;

// Appends a suitable web state test double to the receiver's main interface.
- (void)appendWebStateWithURL:(const GURL&)URL;

// Appends `count` web states, all with `url` as the current URL, to the
- (void)appendWebStatesWithURL:(const GURL&)URL count:(int)count;

// Must be called before -dealloc.
- (void)shutdown;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_TEST_FAKE_SCENE_STATE_H_
