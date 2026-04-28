// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_STATE_SCENE_UI_BLOCKER_STATE_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_STATE_SCENE_UI_BLOCKER_STATE_H_

#import <UIKit/UIKit.h>

@class SceneUIBlockerState;

// Protocol for observers of the scene UI blocker state.
@protocol SceneUIBlockerStateObserver <NSObject>

@optional

// Called when the presentingModalOverlay is about to be set to YES.
- (void)willShowModalOverlay;

// Called when the presentingModalOverlay is about to be set to NO.
- (void)willHideModalOverlay;

// Called when the presentingModalOverlay has been set to NO.
- (void)didHideModalOverlay;

@end

// Object containing the state of the scene UI blocker.
@interface SceneUIBlockerState : NSObject

// When this is YES, the scene is showing the modal overlay.
@property(nonatomic, assign) BOOL presentingModalOverlay;

// Adds observer.
- (void)addObserver:(id<SceneUIBlockerStateObserver>)observer;
// Removes observer.
- (void)removeObserver:(id<SceneUIBlockerStateObserver>)observer;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_STATE_SCENE_UI_BLOCKER_STATE_H_
