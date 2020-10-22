// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_APPLICATION_DELEGATE_APP_STATE_H_
#define IOS_CHROME_APP_APPLICATION_DELEGATE_APP_STATE_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/app/application_delegate/app_state_agent.h"
#import "ios/chrome/browser/ui/main/scene_state_observer.h"
#import "ios/chrome/browser/ui/scoped_ui_blocker/ui_blocker_manager.h"

@class AppState;
@protocol BrowserLauncher;
class ChromeBrowserState;
@class CommandDispatcher;
@protocol ConnectionInformation;
@class SceneState;
@class MainApplicationDelegate;
@class MemoryWarningHelper;
@class MetricsMediator;
@protocol StartupInformation;
@protocol TabOpening;
@protocol TabSwitching;

@protocol AppStateObserver <NSObject>

@optional

// Called when a scene is connected.
// On iOS 12, called when the mainSceneState is set.
- (void)appState:(AppState*)appState sceneConnected:(SceneState*)sceneState;

// Called when the first scene initializes its UI.
- (void)appState:(AppState*)appState
    firstSceneHasInitializedUI:(SceneState*)sceneState;

// Called after the app exits safe mode.
- (void)appStateDidExitSafeMode:(AppState*)appState;

// Called when |AppState.lastTappedWindow| changes.
- (void)appState:(AppState*)appState lastTappedWindowChanged:(UIWindow*)window;

@end

// Represents the application state and responds to application state changes
// and system events.
@interface AppState : NSObject <UIBlockerManager, SceneStateObserver>

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)
initWithBrowserLauncher:(id<BrowserLauncher>)browserLauncher
     startupInformation:(id<StartupInformation>)startupInformation
    applicationDelegate:(MainApplicationDelegate*)applicationDelegate
    NS_DESIGNATED_INITIALIZER;

// Dispatcher for app-level commands for multiwindow use cases.
// Most features should use the browser-level dispatcher instead.
@property(nonatomic, strong) CommandDispatcher* appCommandDispatcher;

// The ChromeBrowserState associated with the main (non-OTR) browsing mode.
@property(nonatomic, assign) ChromeBrowserState* mainBrowserState;

// Container for startup information.
@property(nonatomic, weak) id<StartupInformation> startupInformation;

// YES if the user has ever interacted with the application. May be NO if the
// application has been woken up by the system for background work.
@property(nonatomic, readonly) BOOL userInteracted;

// YES if the sign-in upgrade promo has been presented to the user, once.
@property(nonatomic) BOOL signinUpgradePromoPresentedOnce;

// YES if the default browser fullscreen promo has met the qualifications to be
// shown after the last cold start.
@property(nonatomic) BOOL shouldShowDefaultBrowserPromo;

// When multiwindow is unavailable, this is the only scene state. It is created
// by the app delegate.
@property(nonatomic, strong) SceneState* mainSceneState;

// Indicates that this app launch is one after a crash.
@property(nonatomic, assign) BOOL postCrashLaunch;

// Indicates that session restoration might be required for connecting scenes.
@property(nonatomic, assign) BOOL sessionRestorationRequired;

// The last window which received a tap.
@property(nonatomic, weak) UIWindow* lastTappedWindow;

// The SceneSession ID for the last session, where the Device doesn't support
// multiple windows.
@property(nonatomic, strong) NSString* previousSingleWindowSessionID;

// Saves the launchOptions to be used from -newTabFromLaunchOptions. If the
// application is in background, initialize the browser to basic. If not, launch
// the browser.
// Returns whether additional delegate handling should be performed (call to
// -performActionForShortcutItem or -openURL by the system for example)
- (BOOL)requiresHandlingAfterLaunchWithOptions:(NSDictionary*)launchOptions
                               stateBackground:(BOOL)stateBackground;

// Whether the application is in Safe Mode.
- (BOOL)isInSafeMode;

// Logs duration of the session in the main tab model and records that chrome is
// no longer in cold start.
- (void)willResignActiveTabModel;

// Called when the application is getting terminated. It stops all outgoing
// requests, config updates, clears the device sharing manager and stops the
// mainChrome instance.
- (void)applicationWillTerminate:(UIApplication*)application;

// Called when the application discards set of scene sessions, these sessions
// can no longer be accessed and all their associated data should be destroyed.
- (void)application:(UIApplication*)application
    didDiscardSceneSessions:(NSSet<UISceneSession*>*)sceneSessions
    API_AVAILABLE(ios(13));

// Resumes the session: reinitializing metrics and opening new tab if necessary.
// User sessions are defined in terms of BecomeActive/ResignActive so that
// session boundaries include things like turning the screen off or getting a
// phone call, not just switching apps.
- (void)resumeSessionWithTabOpener:(id<TabOpening>)tabOpener
                       tabSwitcher:(id<TabSwitching>)tabSwitcher
             connectionInformation:
                 (id<ConnectionInformation>)connectionInformation;

// Called when going into the background. iOS already broadcasts, so
// stakeholders can register for it directly.
- (void)applicationDidEnterBackground:(UIApplication*)application
                         memoryHelper:(MemoryWarningHelper*)memoryHelper;

// Called when returning to the foreground. Resets and uploads the metrics.
// Starts the browser to foreground if needed.
- (void)applicationWillEnterForeground:(UIApplication*)application
                       metricsMediator:(MetricsMediator*)metricsMediator
                          memoryHelper:(MemoryWarningHelper*)memoryHelper;

// Sets the return value for -didFinishLaunchingWithOptions that determines if
// UIKit should make followup delegate calls such as
// -performActionForShortcutItem or -openURL.
- (void)launchFromURLHandled:(BOOL)URLHandled;

// Returns the foreground and active scene, if there is one.
- (SceneState*)foregroundActiveScene;

// Returns a list of all connected scenes.
- (NSArray<SceneState*>*)connectedScenes;

// Adds an observer to this app state. The observers will be notified about
// app state changes per AppStateObserver protocol.
- (void)addObserver:(id<AppStateObserver>)observer;
// Removes the observer. It's safe to call this at any time, including from
// AppStateObserver callbacks.
- (void)removeObserver:(id<AppStateObserver>)observer;

// Adds a new agent. Agents are owned by the app state.
// This automatically sets the app state on the |agent|.
- (void)addAgent:(id<AppStateAgent>)agent;

@end

#endif  // IOS_CHROME_APP_APPLICATION_DELEGATE_APP_STATE_H_
