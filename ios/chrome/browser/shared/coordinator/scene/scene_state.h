// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_STATE_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_STATE_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/scene/scene_activation_level.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_observer.h"
#import "ios/chrome/browser/ui/scoped_ui_blocker/ui_blocker_target.h"
#import "ios/chrome/browser/window_activities/model/window_activity_helpers.h"

@class AppState;
@protocol BrowserProviderInterface;
@class ProfileState;
@class SceneController;
@class SceneState;

// Scene agents are objects owned by a scene state and providing some
// scene-scoped function. They can be driven by SceneStateObserver events.
@protocol SceneAgent <NSObject>

@required
// Sets the associated scene state. Called once and only once. Consider using
// this method to add the agent as an observer.
- (void)setSceneState:(SceneState*)scene;

@end

// An object containing the state of a UIWindowScene. One state object
// corresponds to one scene.
// TODO(b/326186137): This class should implement BrowserProviderInterface.
@interface SceneState : NSObject <UIBlockerTarget>

- (instancetype)initWithAppState:(AppState*)appState NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// The app state for the app that owns this scene. Set in init.
@property(nonatomic, weak, readonly) AppState* appState;

// The profile state for profile that owns this scene.
@property(nonatomic, weak) ProfileState* profileState;

// The current activation level.
@property(nonatomic, assign) SceneActivationLevel activationLevel;

// The current origin of the scene.  After window creation this will be
// WindowActivityRestoredOrigin.
@property(nonatomic, assign) WindowActivityOrigin currentOrigin;

// YES if some incognito content is visible, for example an incognito tab or the
// incognito tab switcher.
@property(nonatomic) BOOL incognitoContentVisible;

// Window for the associated scene, if any.
@property(nonatomic, readonly) UIWindow* window;

// The scene object backing this scene state. It's in a 1-to-1 relationship and
// the window scene owns this object (indirectly through scene delegate).
@property(nonatomic, weak) UIWindowScene* scene;

// Connection options of `scene`, if any, from when the scene was connected.
@property(nonatomic, strong) UISceneConnectionOptions* connectionOptions;

// The interface provider associated with this scene.
@property(nonatomic, strong, readonly) id<BrowserProviderInterface>
    browserProviderInterface;

// The persistent identifier for the scene session. This should be used instead
// of -[UISceneSession persistentIdentifier].
@property(nonatomic, readonly) NSString* sceneSessionID;

// The controller for this scene.
@property(nonatomic, weak) SceneController* controller;

// When this is YES, the scene is showing the modal overlay.
@property(nonatomic, assign) BOOL presentingModalOverlay;

// When this is YES, the scene either resumed or started up in response to an
// external intent.
@property(nonatomic, assign) BOOL startupHadExternalIntent;

// URLs passed to `UIWindowSceneDelegate scene:openURLContexts:` that needs to
// be open next time the scene is activated.
// Setting the property to not nil will add the new URL contexts to the set.
// Setting the property to nil will clear the set.
@property(nonatomic) NSSet<UIOpenURLContext*>* URLContextsToOpen;

// A NSUserActivity that has been passed to
// `UISceneDelegate scene:continueUserActivity:` and needs to be opened.
@property(nonatomic) NSUserActivity* pendingUserActivity;

// YES if the UI is enabled. The browser UI objects are available when this is
// YES.
@property(nonatomic, assign) BOOL UIEnabled;

// YES if the QR scanner is visible.
@property(nonatomic, assign) BOOL QRScannerVisible;

// YES if sign-in is in progress which covers the authentication flow and the
// sign-in prompt UI.
@property(nonatomic, assign) BOOL signinInProgress;

// Adds an observer to this scene state. The observers will be notified about
// scene state changes per SceneStateObserver protocol.
- (void)addObserver:(id<SceneStateObserver>)observer;
// Removes the observer. It's safe to call this at any time, including from
// SceneStateObserver callbacks.
- (void)removeObserver:(id<SceneStateObserver>)observer;

// Adds a new agent. Agents are owned by the scene state.
- (void)addAgent:(id<SceneAgent>)agent;

// Array of all agents added to this scene state.
- (NSArray*)connectedAgents;

// Retrieves per-session preference for `key`. May return nil if the key is
// not found.
- (NSObject*)sessionObjectForKey:(NSString*)key;

// Stores `object` as a per-session preference if supported by the device or
// into NSUserDefaults otherwise (old table, phone, ...).
- (void)setSessionObject:(NSObject*)object forKey:(NSString*)key;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_STATE_H_
