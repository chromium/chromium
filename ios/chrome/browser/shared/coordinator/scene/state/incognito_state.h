// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_STATE_INCOGNITO_STATE_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_STATE_INCOGNITO_STATE_H_

#import <Foundation/Foundation.h>

#include "ios/chrome/browser/shared/coordinator/scene/scene_state.h"

@protocol IncognitoStateObserver <NSObject>

@optional

// Called right before entering incognito.
- (void)willEnterIncognitoForState:(IncognitoState*)incognitoState;

// Called right before exiting incognito.
- (void)willExitIncognitoForState:(IncognitoState*)incognitoState;

@end

@interface IncognitoState : NSObject

@property(nonatomic, assign) BOOL incognitoContentVisible;

@property(nonatomic, weak, readonly) SceneState* sceneState;

- (instancetype)initWithSceneState:(SceneState*)sceneState;

- (instancetype)init NS_UNAVAILABLE;

// Adds observer.
- (void)addObserver:(id<IncognitoStateObserver>)observer;
// Removes observer.
- (void)removeObserver:(id<IncognitoStateObserver>)observer;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_STATE_INCOGNITO_STATE_H_
