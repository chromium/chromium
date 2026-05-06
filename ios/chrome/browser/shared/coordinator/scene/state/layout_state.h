// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_STATE_LAYOUT_STATE_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_STATE_LAYOUT_STATE_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/scene/state/layout_transition_coordinating.h"

// The position of the app bar.
enum class AppBarPosition {
  kNone,
  kBottom,
  kLeft,
  kRight,
};

@class LayoutState;

// Protocol for observers of the layout state.
@protocol LayoutStateObserver <NSObject>

@optional

// Called when the contained layout state is about to change.
// Observers can use the coordinator to animate alongside the transition.
// If the change is not animated, `coordinator` will be `nil`, and observers
// should apply changes immediately.
- (void)layoutState:(LayoutState*)layoutState
    willChangeContainedLayout:(BOOL)containedLayoutActive
    withTransitionCoordinator:(id<LayoutTransitionCoordinating>)coordinator;

// Called when the contained layout supported state changes.
- (void)layoutState:(LayoutState*)layoutState
    didChangeContainedLayoutSupported:(BOOL)supported;

// Called when the windowed mode state changes.
- (void)layoutState:(LayoutState*)layoutState
    didChangeWindowedMode:(BOOL)windowedMode;

// Called when the App Bar position changes.
- (void)layoutState:(LayoutState*)layoutState
    didChangeAppBarPosition:(AppBarPosition)appBarPosition;

@end

// Object containing the state of the layout.
// This class acts as a pure state holder and transition coordinator.
// Observers reacting to state changes will have their UI updates captured by
// the caller's animation block or transition coordinator.
@interface LayoutState : NSObject

// Indicates whether the contained layout is active.
@property(nonatomic, assign) BOOL containedLayoutActive;

// Indicates whether the contained layout is supported in the current
// environment. Updated by `SceneViewController` in response to trait changes.
@property(nonatomic, assign) BOOL containedLayoutSupported;

// Indicates whether the app is in windowed mode (multitasking).
@property(nonatomic, assign) BOOL windowedMode;

// The position of the app bar.
@property(nonatomic, assign) AppBarPosition appBarPosition;

// Sets `containedLayoutActive` with a transition coordinator to
// synchronize animations. `coordinator` must not be nil.
- (void)setContainedLayoutActive:(BOOL)active
       withTransitionCoordinator:(id<LayoutTransitionCoordinating>)coordinator;

// Updates the AppBar position, based on the interfaceOrientation of the window
// scene, and any rotation transforms applied by the transition coordinator.
- (void)updateAppBarPositionWithView:(UIView*)view
                         coordinator:(id<UIViewControllerTransitionCoordinator>)
                                         coordinator;

// Adds an observer to be notified of layout state changes.
- (void)addObserver:(id<LayoutStateObserver>)observer;
// Removes a previously added observer.
- (void)removeObserver:(id<LayoutStateObserver>)observer;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_STATE_LAYOUT_STATE_H_
