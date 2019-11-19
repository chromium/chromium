// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/main_application_delegate.h"

#include "base/ios/ios_util.h"
#include "base/mac/foundation_util.h"
#import "ios/chrome/app/application_delegate/app_navigation.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/browser_launcher.h"
#import "ios/chrome/app/application_delegate/memory_warning_helper.h"
#import "ios/chrome/app/application_delegate/metrics_mediator.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/app/application_delegate/tab_opening.h"
#import "ios/chrome/app/application_delegate/tab_switching.h"
#import "ios/chrome/app/application_delegate/url_opener.h"
#import "ios/chrome/app/application_delegate/user_activity_handler.h"
#import "ios/chrome/app/chrome_overlay_window.h"
#import "ios/chrome/app/main_application_delegate_testing.h"
#import "ios/chrome/app/main_controller.h"
#import "ios/chrome/browser/ui/main/scene_controller.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#include "ios/chrome/browser/ui/util/multi_window_support.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"
#import "ios/testing/perf/startupLoggers.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface MainApplicationDelegate () {
  MainController* _mainController;
  // Memory helper used to log the number of memory warnings received.
  MemoryWarningHelper* _memoryHelper;
  // Metrics mediator used to check and update the metrics accordingly to
  // to the user preferences.
  MetricsMediator* _metricsMediator;
  // Browser launcher to have a global launcher.
  id<BrowserLauncher> _browserLauncher;
  // Container for startup information.
  id<StartupInformation> _startupInformation;
  // Helper to open new tabs.
  id<TabOpening> _tabOpener;
  // Handles the application stage changes.
  AppState* _appState;
  // Handles tab switcher.
  id<AppNavigation> _appNavigation;
  // Handles tab switcher.
  id<TabSwitching> _tabSwitcherProtocol;
}

// The state representing the only "scene" on iOS 12. On iOS 13, only created
// temporarily before multiwindow is fully implemented to also represent the
// only scene.
@property(nonatomic, strong) SceneState* sceneState;

// The controller for |sceneState|.
@property(nonatomic, strong) SceneController* sceneController;

@end

@implementation MainApplicationDelegate

- (instancetype)init {
  if (self = [super init]) {
    _memoryHelper = [[MemoryWarningHelper alloc] init];
    _mainController = [[MainController alloc] init];
    _metricsMediator = [[MetricsMediator alloc] init];
    [_mainController setMetricsMediator:_metricsMediator];
    _browserLauncher = _mainController;
    _startupInformation = _mainController;
    _tabOpener = _mainController;
    _appState = [[AppState alloc] initWithBrowserLauncher:_browserLauncher
                                       startupInformation:_startupInformation
                                      applicationDelegate:self];
    _tabSwitcherProtocol = _mainController;
    _appNavigation = _mainController;
    [_mainController setAppState:_appState];

    if (!IsMultiwindowSupported()) {
      // When multiwindow is not supported, this object holds a "scene" state
      // and a "scene" controller. This allows the rest of the app to be mostly
      // multiwindow-agnostic.
      _sceneState = [[SceneState alloc] init];
      _sceneController =
          [[SceneController alloc] initWithSceneState:_sceneState];

      // This is temporary plumbing that's not supposed to be here.
      _sceneController.mainController = (id<MainControllerGuts>)_mainController;
      _mainController.sceneController = _sceneController;
    }
  }
  return self;
}

- (UIWindow*)window {
  return self.sceneState.window;
}

- (void)setWindow:(UIWindow*)newWindow {
  NOTREACHED() << "Should not be called, use [SceneState window] instead";
}

#pragma mark - UIApplicationDelegate methods -

#pragma mark Responding to App State Changes and System Events

// Called by the OS to create the UI for display.  The UI will not be displayed,
// even if it is ready, until this function returns.
// The absolute minimum work should be done here, to ensure that the application
// startup is fast, and the UI appears as soon as possible.
- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
  startup_loggers::RegisterAppDidFinishLaunchingTime();

  _mainController.window = self.window;
  // self.window has been set by this time. _appState window can now be set.
  _appState.window = self.window;

  BOOL inBackground =
      [application applicationState] == UIApplicationStateBackground;
  BOOL requiresHandling =
      [_appState requiresHandlingAfterLaunchWithOptions:launchOptions
                                        stateBackground:inBackground];
  if (!IsMultiwindowSupported()) {
    self.sceneState.activationLevel = SceneActivationLevelForegroundInactive;
  }

  return requiresHandling;
}

- (void)applicationDidBecomeActive:(UIApplication*)application {
  if (!IsMultiwindowSupported()) {
    self.sceneState.activationLevel = SceneActivationLevelForegroundActive;
  }

  startup_loggers::RegisterAppDidBecomeActiveTime();
  if ([_appState isInSafeMode])
    return;

  [_appState resumeSessionWithTabOpener:_tabOpener
                            tabSwitcher:_tabSwitcherProtocol];
}

- (void)applicationWillResignActive:(UIApplication*)application {
  if (!IsMultiwindowSupported()) {
    self.sceneState.activationLevel = SceneActivationLevelForegroundInactive;
  }

  if ([_appState isInSafeMode])
    return;

  [_appState willResignActiveTabModel];
}

// Called when going into the background. iOS already broadcasts, so
// stakeholders can register for it directly.
- (void)applicationDidEnterBackground:(UIApplication*)application {
  if (!IsMultiwindowSupported()) {
    self.sceneState.activationLevel = SceneActivationLevelBackground;
  }

  [_appState applicationDidEnterBackground:application
                              memoryHelper:_memoryHelper
                   incognitoContentVisible:self.sceneController
                                               .incognitoContentVisible];
}

// Called when returning to the foreground.
- (void)applicationWillEnterForeground:(UIApplication*)application {
  if (!IsMultiwindowSupported()) {
    self.sceneState.activationLevel = SceneActivationLevelForegroundInactive;
  }

  [_appState applicationWillEnterForeground:application
                            metricsMediator:_metricsMediator
                               memoryHelper:_memoryHelper
                                  tabOpener:_tabOpener
                              appNavigation:_appNavigation];
}

- (void)applicationWillTerminate:(UIApplication*)application {
  if ([_appState isInSafeMode])
    return;

  // Instead of adding code here, consider if it could be handled by listening
  // for  UIApplicationWillterminate.
  [_appState applicationWillTerminate:application
                applicationNavigation:_appNavigation];
}

- (void)applicationDidReceiveMemoryWarning:(UIApplication*)application {
  if ([_appState isInSafeMode])
    return;

  [_memoryHelper handleMemoryPressure];
}

#pragma mark Downloading Data in the Background

- (void)application:(UIApplication*)application
    handleEventsForBackgroundURLSession:(NSString*)identifier
                      completionHandler:(void (^)(void))completionHandler {
  if ([_appState isInSafeMode])
    return;
  // This initialization to BACKGROUND stage may not be necessary, but is
  // preserved in case somewhere there is a dependency on this.
  [_browserLauncher startUpBrowserToStage:INITIALIZATION_STAGE_BACKGROUND];
  completionHandler();
}

#pragma mark Continuing User Activity and Handling Quick Actions

- (BOOL)application:(UIApplication*)application
    willContinueUserActivityWithType:(NSString*)userActivityType {
  if ([_appState isInSafeMode])
    return NO;

  // Enusre Chrome is fuilly started up in case it had launched to the
  // background.
  [_browserLauncher startUpBrowserToStage:INITIALIZATION_STAGE_FOREGROUND];

  return
      [UserActivityHandler willContinueUserActivityWithType:userActivityType];
}

- (BOOL)application:(UIApplication*)application
    continueUserActivity:(NSUserActivity*)userActivity
      restorationHandler:
          (void (^)(NSArray<id<UIUserActivityRestoring>>*))restorationHandler {
  if ([_appState isInSafeMode])
    return NO;

  // Enusre Chrome is fuilly started up in case it had launched to the
  // background.
  [_browserLauncher startUpBrowserToStage:INITIALIZATION_STAGE_FOREGROUND];

  BOOL applicationIsActive =
      [application applicationState] == UIApplicationStateActive;

  return [UserActivityHandler continueUserActivity:userActivity
                               applicationIsActive:applicationIsActive
                                         tabOpener:_tabOpener
                                startupInformation:_startupInformation];
}

- (void)application:(UIApplication*)application
    performActionForShortcutItem:(UIApplicationShortcutItem*)shortcutItem
               completionHandler:(void (^)(BOOL succeeded))completionHandler {
  if ([_appState isInSafeMode])
    return;

  // Enusre Chrome is fuilly started up in case it had launched to the
  // background.
  [_browserLauncher startUpBrowserToStage:INITIALIZATION_STAGE_FOREGROUND];

  [UserActivityHandler
      performActionForShortcutItem:shortcutItem
                 completionHandler:completionHandler
                         tabOpener:_tabOpener
                startupInformation:_startupInformation
                 interfaceProvider:_mainController.interfaceProvider];
}

#pragma mark Opening a URL-Specified Resource

// Handles open URL. The registered URL Schemes are defined in project
// variables ${CHROMIUM_URL_SCHEME_x}.
// The url can either be empty, in which case the app is simply opened or
// can contain an URL that will be opened in a new tab.
- (BOOL)application:(UIApplication*)application
            openURL:(NSURL*)url
            options:(NSDictionary<NSString*, id>*)options {
  if ([_appState isInSafeMode])
    return NO;

  // The various URL handling mechanisms require that the application has
  // fully started up; there are some cases (crbug.com/658420) where a
  // launch via this method crashes because some services (specifically,
  // CommandLine) aren't initialized yet. So: before anything further is
  // done, make sure that Chrome is fully started up.
  [_browserLauncher startUpBrowserToStage:INITIALIZATION_STAGE_FOREGROUND];

  if (ios::GetChromeBrowserProvider()
          ->GetChromeIdentityService()
          ->HandleApplicationOpenURL(application, url, options)) {
    return YES;
  }

  BOOL applicationActive =
      [application applicationState] == UIApplicationStateActive;

  return [URLOpener openURL:url
          applicationActive:applicationActive
                    options:options
                  tabOpener:_tabOpener
         startupInformation:_startupInformation];
}

#pragma mark - Testing methods

- (MainController*)mainController {
  return _mainController;
}

- (AppState*)appState {
  return _appState;
}

+ (MainController*)sharedMainController {
  return base::mac::ObjCCast<MainApplicationDelegate>(
             [[UIApplication sharedApplication] delegate])
      .mainController;
}

@end
