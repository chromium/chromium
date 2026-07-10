// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_STATE_LAYOUT_STATE_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_STATE_LAYOUT_STATE_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/scene/state/layout_state_passkey.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/layout_transition_coordinating.h"

// The position of the app bar.
enum class AppBarPosition {
  kNone,
  kBottom,
  kLeft,
  kRight,
};

// The position of the main toolbar (omnibox).
enum class ToolbarPosition {
  kTop,
  kBottom,
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

// Called when the assistant container cutout radius changes.
- (void)layoutState:(LayoutState*)layoutState
    didChangeAssistantContainerCutoutRadius:
        (CGFloat)assistantContainerCutoutRadius;

// Called when the toolbar position changes.
- (void)layoutState:(LayoutState*)layoutState
    didChangeToolbarPosition:(ToolbarPosition)toolbarPosition;

// Called when the Gemini Floaty invocation state changes.
- (void)layoutState:(LayoutState*)layoutState
    didChangeGeminiFloatyInvoked:(BOOL)geminiFloatyInvoked;

@end

// Object containing the state of the layout.
// This class acts as a pure state holder and transition coordinator.
// Observers reacting to state changes will have their UI updates captured by
// the caller's animation block or transition coordinator.
@interface LayoutState : NSObject

// Indicates whether the contained layout is active.
@property(nonatomic, readonly) BOOL containedLayoutActive;

// Indicates whether the contained layout is supported in the current
// environment. Updated by `SceneViewController` in response to trait changes.
@property(nonatomic, readonly) BOOL containedLayoutSupported;

// Indicates whether the app is in windowed mode (multitasking).
@property(nonatomic, readonly) BOOL windowedMode;

// The position of the app bar.
@property(nonatomic, readonly) AppBarPosition appBarPosition;

// The cutout corner radius of the App Bar matching the assistant container.
@property(nonatomic, readonly) CGFloat assistantContainerCutoutRadius;

// The position of the toolbar (omnibox).
@property(nonatomic, readonly) ToolbarPosition toolbarPosition;

// Indicates whether the Gemini Floaty is currently invoked.
@property(nonatomic, readonly) BOOL geminiFloatyInvoked;

// Custom setters requiring domain-level passkeys.
- (void)setContainedLayoutActive:(BOOL)active
                    scenePassKey:(LayoutStateScenePassKey)passKey;
- (void)setContainedLayoutActive:(BOOL)active
                assistantPassKey:(LayoutStateAssistantPassKey)passKey;
- (void)setContainedLayoutSupported:(BOOL)supported
                            passKey:(LayoutStateScenePassKey)passKey;
- (void)setWindowedMode:(BOOL)windowedMode
                passKey:(LayoutStateScenePassKey)passKey;
- (void)setAssistantContainerCutoutRadius:(CGFloat)radius
                                  passKey:(LayoutStateAssistantPassKey)passKey;
- (void)setGeminiFloatyInvoked:(BOOL)invoked
                       passKey:(LayoutStateAssistantPassKey)passKey;
- (void)setToolbarPosition:(ToolbarPosition)position
                   passKey:(LayoutStateToolbarPassKey)passKey;
- (void)setAppBarPosition:(AppBarPosition)position
                  passKey:(LayoutStateScenePassKey)passKey;

// Transition setters secured by domain-level passkeys.
- (void)setContainedLayoutActive:(BOOL)active
       withTransitionCoordinator:(id<LayoutTransitionCoordinating>)coordinator
                    scenePassKey:(LayoutStateScenePassKey)passKey;
- (void)setContainedLayoutActive:(BOOL)active
       withTransitionCoordinator:(id<LayoutTransitionCoordinating>)coordinator
                assistantPassKey:(LayoutStateAssistantPassKey)passKey;

// Updates the AppBar position, based on the interfaceOrientation of the window
// scene, and any rotation transforms applied by the transition coordinator.
- (void)updateAppBarPositionWithView:(UIView*)view
                         coordinator:(id<UIViewControllerTransitionCoordinator>)
                                         coordinator
                             passKey:(LayoutStateScenePassKey)passKey;

// Adds an observer to be notified of layout state changes.
- (void)addObserver:(id<LayoutStateObserver>)observer;
// Removes a previously added observer.
- (void)removeObserver:(id<LayoutStateObserver>)observer;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_STATE_LAYOUT_STATE_H_
