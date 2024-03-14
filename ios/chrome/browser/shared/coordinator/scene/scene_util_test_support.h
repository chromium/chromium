// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_UTIL_TEST_SUPPORT_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_UTIL_TEST_SUPPORT_H_

#import <UIKit/UIKit.h>
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"

@class AppState;

// Returns a fake UIScene with `identifier` as session persistent identifier
// when running on iOS 13+ or nil otherwise. The fake object implements just
// enough API for SessionIdentifierForScene().
id FakeSceneWithIdentifier(NSString* identifier);

// Returns a SceneState object with an underlying fake UIScene.
@interface SceneStateWithFakeScene : SceneState

- (instancetype)initWithScene:(id)scene
                     appState:(AppState*)appState NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithAppState:(AppState*)appState NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_UTIL_TEST_SUPPORT_H_
