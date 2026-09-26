// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/background_launch_app_interface.h"

#import <BackgroundTasks/BackgroundTasks.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/background_refresh/background_refresh_app_agent+Testing.h"
#import "ios/chrome/app/background_refresh/background_refresh_app_agent.h"
#import "ios/chrome/app/main_application_delegate.h"
#import "ios/chrome/app/main_application_delegate_testing.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"

// Minimal BGTask stand-in used when simulating background refresh in tests.
// `BGTask` cannot be instantiated directly, so the agent is handed this object
// instead; it only needs to respond to the subset of `BGTask` used by the
// refresh flow.
@interface SimulatedBGTask : NSObject
@property(nonatomic, copy) void (^expirationHandler)(void);
- (void)setTaskCompletedWithSuccess:(BOOL)success;
@end

@implementation SimulatedBGTask
- (void)setTaskCompletedWithSuccess:(BOOL)success {
}
@end

@implementation BackgroundLaunchAppInterface

#pragma mark - Public

+ (void)unblockStartupAndForeground {
  tests_hook::UnpauseStartupAtBackgroundStage();
  AppState* appState = MainApplicationDelegate.sharedAppState;
  [appState queueTransitionToNextInitStage];
  for (SceneState* sceneState in appState.connectedScenes) {
    sceneState.currentOrigin = WindowActivityRestoredOrigin;
    sceneState.activationLevel = SceneActivationLevelForegroundInactive;
    sceneState.activationLevel = SceneActivationLevelForegroundActive;
  }
}

+ (void)simulateBackgroundRefresh {
  AppState* appState = MainApplicationDelegate.sharedAppState;
  BackgroundRefreshAppAgent* refreshAgent =
      [BackgroundRefreshAppAgent agentFromApp:appState];
  SimulatedBGTask* simulatedTask = [[SimulatedBGTask alloc] init];
  [refreshAgent simulateRefreshWithTask:(BGTask*)simulatedTask];
}

+ (void)simulateBackgroundURLSession {
  MainApplicationDelegate* appDelegate =
      (MainApplicationDelegate*)UIApplication.sharedApplication.delegate;
  [appDelegate application:UIApplication.sharedApplication
      handleEventsForBackgroundURLSession:@"test_session"
                        completionHandler:^{
                        }];
}

+ (void)simulateBackgroundSilentNotification {
  MainApplicationDelegate* appDelegate =
      (MainApplicationDelegate*)UIApplication.sharedApplication.delegate;
  [appDelegate application:UIApplication.sharedApplication
      didReceiveRemoteNotification:@{}
            fetchCompletionHandler:^(UIBackgroundFetchResult result){
            }];
}

@end
