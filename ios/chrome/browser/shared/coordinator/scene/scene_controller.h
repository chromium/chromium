// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "base/functional/callback.h"
#import "ios/chrome/app/application_delegate/tab_opening.h"
#import "ios/chrome/browser/shared/coordinator/scene/connection_information.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_observer.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"

@protocol BrowserProviderInterface;
@class ProfileState;

// The controller object for a scene. Reacts to scene state changes.
@interface SceneController : NSObject <SceneStateObserver,
                                       ApplicationCommands,
                                       SettingsCommands,
                                       ConnectionInformation,
                                       TabOpening,
                                       WebStateListObserving>

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithSceneState:(SceneState*)sceneState
    NS_DESIGNATED_INITIALIZER;

// The interface provider for this scene.
@property(nonatomic, strong, readonly) id<BrowserProviderInterface>
    browserProviderInterface;

// YES if the tab grid is the main user interface at the moment.
@property(nonatomic, readonly, getter=isTabGridVisible) BOOL tabGridVisible;

// Connects the ProfileState to this SceneController.
- (void)setProfileState:(ProfileState*)profileState;

// Handler for the UIWindowSceneDelegate callback with the same selector.
- (void)performActionForShortcutItem:(UIApplicationShortcutItem*)shortcutItem
                   completionHandler:
                       (void (^)(BOOL succeeded))completionHandler;

// This method completely destroys all of the UI. It should be called when the
// scene is disconnected.
// It should not be called directly, except by unit test that
// don’t test the whole activation level life cycle.
- (void)teardownUI;
@end

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_CONTROLLER_H_
