// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_APPLICATION_DELEGATE_APP_STATE_H_
#define IOS_CHROME_APP_APPLICATION_DELEGATE_APP_STATE_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/app/application_delegate/app_init_stage.h"
#import "ios/chrome/app/application_delegate/app_state_agent.h"
#import "ios/chrome/app/application_delegate/app_state_observer.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_observer.h"
#import "ios/chrome/browser/ui/device_orientation/portait_orientation_manager.h"
#import "ios/chrome/browser/ui/scoped_ui_blocker/ui_blocker_manager.h"

@class CommandDispatcher;
@class SceneState;
@class MemoryWarningHelper;
@class MetricsMediator;
@class ProfileState;
@protocol StartupInformation;

namespace base {
class TimeTicks;
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PostCrashAction {
  // Restore tabs normally after a clean shutdown.
  kRestoreTabsCleanShutdown = 0,
  // Restore tabs normally after an unclean shutdown.
  kRestoreTabsUncleanShutdown = 1,
  // kStashTabsAndShowNTP is no longer used, but the value 2 cannot be reused
  // as it would break histograms.
  // Restore tabs with `return to previous tab` NTP.
  kShowNTPWithReturnToTab = 3,
  // Show safe mode.
  kShowSafeMode = 4,
  kMaxValue = kShowSafeMode,
};

// Represents the application state and responds to application state changes
// and system events.
@interface AppState : NSObject <PortraitOrientationManager,
                                SceneStateObserver,
                                UIBlockerManager>

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithStartupInformation:
    (id<StartupInformation>)startupInformation NS_DESIGNATED_INITIALIZER;

// Dispatcher for app-level commands for multiwindow use cases.
// Most features should use the browser-level dispatcher instead.
@property(nonatomic, strong) CommandDispatcher* appCommandDispatcher;

// The ProfileState associated with the main Profile.
// TODO(crbug.com/324417250) remove this property.
@property(nonatomic, weak) ProfileState* mainProfile;

// Container for startup information.
@property(nonatomic, weak) id<StartupInformation> startupInformation;

// YES if the user has ever interacted with the application. May be NO if the
// application has been woken up by the system for background work.
@property(nonatomic, readonly) BOOL userInteracted;

// YES if the sign-in upgrade promo has been presented to the user, once.
@property(nonatomic) BOOL signinUpgradePromoPresentedOnce;

// YES if the sign-out prompt should be shown to the user when the scene becomes
// active and enters the foreground. This can happen if the policies have
// changed since the last cold start, meaning the user was signed out during
// startup.
@property(nonatomic) BOOL shouldShowForceSignOutPrompt;

// Indicates what action, if any, is taken after a crash (stash tabs, show NTP,
// show safe mode).
@property(nonatomic, assign) PostCrashAction postCrashAction;

// YES if the app is resuming from safe mode.
@property(nonatomic) BOOL resumingFromSafeMode;

// Timestamp of when a scene was last becoming active. Can be null.
@property(nonatomic, assign) base::TimeTicks lastTimeInForeground;

// The initialization stage the app is currently at.
@property(nonatomic, readonly) AppInitStage initStage;

// This flag is set when the first scene has initialized its UI and never
// resets.
@property(nonatomic, readonly) BOOL firstSceneHasInitializedUI;

// YES if the views being presented should only support the portrait
// orientation.
@property(nonatomic, readonly) BOOL portraitOnly;

// YES if the application is getting terminated.
@property(nonatomic, readonly) BOOL appIsTerminating;
@property(nonatomic, assign, readwrite) BOOL overridePortraitOnly;

// All agents that have been attached. Use -addAgent: and -removeAgent: to
// add and remove agents.
@property(nonatomic, readonly) NSArray<id<AppStateAgent>>* connectedAgents;

// Logs duration of the session and records that chrome is no longer in cold
// start.
- (void)willResignActive;

// Called when the application is getting terminated. It stops all outgoing
// requests, config updates, clears the device sharing manager and stops the
// mainChrome instance.
- (void)applicationWillTerminate:(UIApplication*)application;

// Called when the application discards set of scene sessions, these sessions
// can no longer be accessed and all their associated data should be destroyed.
- (void)application:(UIApplication*)application
    didDiscardSceneSessions:(NSSet<UISceneSession*>*)sceneSessions;

// Called when going into the background. iOS already broadcasts, so
// stakeholders can register for it directly.
- (void)applicationDidEnterBackground:(UIApplication*)application
                         memoryHelper:(MemoryWarningHelper*)memoryHelper;

// Called when returning to the foreground. Resets and uploads the metrics.
// Starts the browser to foreground if needed.
- (void)applicationWillEnterForeground:(UIApplication*)application
                       metricsMediator:(MetricsMediator*)metricsMediator
                          memoryHelper:(MemoryWarningHelper*)memoryHelper;

// Returns the foreground and active scene, if there is one.
- (SceneState*)foregroundActiveScene;

// Returns a list of all connected scenes.
- (NSArray<SceneState*>*)connectedScenes;

// Returns a list of all scenes in the foreground that are not necessarly
// active.
- (NSArray<SceneState*>*)foregroundScenes;

// Adds an observer to this app state. The observers will be notified about
// app state changes per AppStateObserver protocol.
// The observer will be *immediately* notified about the latest init stage
// transition, if any such transitions happened (didTransitionFromInitStage),
// before this method returns.
- (void)addObserver:(id<AppStateObserver>)observer;
// Removes the observer. It's safe to call this at any time, including from
// AppStateObserver callbacks.
- (void)removeObserver:(id<AppStateObserver>)observer;

// Adds a new agent. Agents are owned by the app state.
// This automatically sets the app state on the `agent`.
- (void)addAgent:(id<AppStateAgent>)agent;
// Removes an agent.
- (void)removeAgent:(id<AppStateAgent>)agent;

// Queue the transition to the next app initialization stage. Will stop
// transitioning when the Final stage is reached.
// All observers will be notified about each transition until the next
// transition takes place. If an observer calls this method from a transition
// notification, the method will return, the observers will be notified of the
// prior change, and then transition will take place. Then this method will
// finally return to the runloop. It is an error to queue more than one
// transition at once.
- (void)queueTransitionToNextInitStage;

// Queue the transition (as defined above) to the very first initialization
// stage.
- (void)startInitialization;

@end

#endif  // IOS_CHROME_APP_APPLICATION_DELEGATE_APP_STATE_H_
